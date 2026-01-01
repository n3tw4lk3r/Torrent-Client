#include "core/TorrentClient.hpp"
#include "net/PeerConnect.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <algorithm>

using namespace std::chrono_literals;

TorrentClient::TorrentClient(const std::string& peerId) :
    peer_id(peerId + GenerateRandomSuffix())
{
    current_task.start_time = std::chrono::system_clock::now();
    AddLogMessage("Torrent client initialized");
}

std::string TorrentClient::GenerateRandomSuffix(size_t length) {
    static std::random_device random;
    static std::mt19937 gen(random());
    static std::uniform_int_distribution<> distribution('A', 'Z');

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(static_cast<char>(distribution(gen)));
    }
    return result;
}

bool TorrentClient::RunDownloadMultithread(PieceStorage& pieces,
                                           const TorrentFile& torrent_file,
                                           const TorrentTracker& tracker) {
    std::vector<std::thread> peer_threads;
    std::vector<std::shared_ptr<PeerConnect>> peer_connections;

    for (const Peer& peer : tracker.GetPeers()) {
        try {
            peer_connections.emplace_back(
                std::make_shared<PeerConnect>(peer, torrent_file, peer_id, pieces)
            );
        } catch (const std::exception& e) {
            std::cerr << "Failed to create connection to " << peer.ip << ":" << peer.port
                      << " - " << e.what() << std::endl;
        }
    }

    if (peer_connections.empty()) {
        std::cerr << "No valid peer connections established" << std::endl;
        return true;
    }

    peer_threads.reserve(peer_connections.size());
    for (auto& peer_connect_ptr : peer_connections) {
        peer_threads.emplace_back([peer_connect_ptr]() {
            while (!peer_connect_ptr->IsTerminated()) {
                try {
                    peer_connect_ptr->Run();
                } catch (const std::exception& e) {
                    std::cerr << "Peer " << " error: "
                              << e.what() << " - reconnecting..." << std::endl;

                    if (!peer_connect_ptr->IsTerminated()) {
                        std::this_thread::sleep_for(5s);
                    }
                }
            }
            std::cout << "Peer thread for " << " terminated" << std::endl;
        });
    }

    std::cout << "Started " << peer_threads.size() << " threads for peers" << std::endl;

    const size_t target_pieces = pieces.TotalPiecesCount();

    std::cout << "=== DOWNLOAD STARTED ===" << std::endl;
    std::cout << "Target pieces: " << target_pieces << std::endl;
    std::cout << "Initial saved pieces: " << pieces.PiecesSavedToDiscCount() << std::endl;

    bool endgame_mode = false;
    auto last_requeue_time = std::chrono::steady_clock::now();
    const auto requeue_interval = std::chrono::seconds(10);

    while (!is_terminated && !pieces.IsDownloadComplete()) {
        size_t missing_count = pieces.GetMissingPieces().size();

        if (!endgame_mode && missing_count <= 10) {
            endgame_mode = true;
            std::cout << "=== ENTERING ENDGAME MODE ===" << std::endl;
            std::cout << "Missing pieces: " << missing_count << std::endl;
        }

        if (endgame_mode && pieces.QueueIsEmpty()) {
            auto now = std::chrono::steady_clock::now();
            if (now - last_requeue_time > requeue_interval) {
                std::cout << "ENDGAME: Forcing requeue of " << missing_count << " missing pieces" << std::endl;
                pieces.ForceRequeueMissingPieces();
                last_requeue_time = now;
            }
        }

        if (!pieces.HasActiveWork()) {
            auto missing = pieces.GetMissingPieces();
            std::cout << "No active work. Missing: " << missing.size()
                      << ", Queue: " << (pieces.QueueIsEmpty() ? "empty" : "has work")
                      << (endgame_mode ? " [ENDGAME]" : "") << std::endl;

            std::this_thread::sleep_for(500ms);
        } else {
            std::this_thread::sleep_for(1000ms);
        }

        size_t current_saved_count = pieces.PiecesSavedToDiscCount();
        if (current_saved_count % 5 == 0 || current_saved_count == target_pieces || endgame_mode) {
            std::cout << "Progress: " << current_saved_count << "/" << target_pieces
                      << (endgame_mode ? " [ENDGAME]" : "") << std::endl;
        }
    }

    std::cout << "=== DOWNLOAD FINISHED ===" << std::endl;
    std::cout << "Final saved pieces: " << pieces.PiecesSavedToDiscCount() << "/" << target_pieces << std::endl;
    std::cout << "Download complete: " << (pieces.IsDownloadComplete() ? "YES" : "NO") << std::endl;

    std::cout << "Terminating peer connections..." << std::endl;

    is_terminated = true;

    for (auto& peer_connect_ptr : peer_connections) {
        peer_connect_ptr->Terminate();
    }

    for (auto& thread : peer_threads) {
        if (thread.joinable()) {
            thread.join();
        } else {
            thread.detach();
        }
    }

    std::cout << "=== FINAL DIAGNOSTICS ===" << std::endl;
    pieces.PrintMissingPieces();

    bool download_complete = pieces.IsDownloadComplete();
    if (download_complete) {
        std::cout << "DOWNLOAD COMPLETE SUCCESSFULLY!" << std::endl;
    } else {
        std::cout << "DOWNLOAD INCOMPLETE! Missing pieces: "
                  << (target_pieces - pieces.PiecesSavedToDiscCount())
                  << std::endl;
    }

    return !download_complete;
}

void TorrentClient::DownloadFromTracker(const TorrentFile& torrent_file, PieceStorage& pieces) {
    std::vector<std::string> trackers = {
        torrent_file.announce,
        "udp://tracker.opentrackr.org:1337/announce",
        "udp://open.stealth.si:80/announce",
        "udp://exodus.desync.com:6969/announce",
        "udp://tracker.torrent.eu.org:451/announce"
    };

    std::sort(trackers.begin(), trackers.end());
    trackers.erase(std::unique(trackers.begin(), trackers.end()), trackers.end());

    std::cout << "Using " << trackers.size() << " unique trackers" << std::endl;

    int retry_count = 0;
    const int max_retries = 10;

    while (!is_terminated && !pieces.IsDownloadComplete()) {
        std::vector<Peer> all_peers;

        for (size_t i = 0; i < trackers.size() && !is_terminated; ++i) {
            try {
                TorrentTracker tracker(trackers[i]);
                tracker.UpdatePeers(torrent_file, peer_id, 12345);
                const auto& peers = tracker.GetPeers();
                all_peers.insert(all_peers.end(), peers.begin(), peers.end());
                std::cout << trackers[i] << " - " << peers.size() << " peers" << std::endl;
            } catch (const std::exception& e) {
                std::cout << trackers[i] << " - " << e.what() << std::endl;
            }
        }

        if (!all_peers.empty()) {
            std::sort(all_peers.begin(), all_peers.end(), [](const Peer& a, const Peer& b) {
                return a.ip < b.ip || (a.ip == b.ip && a.port < b.port);
            });
            auto last = std::unique(all_peers.begin(), all_peers.end(), [](const Peer& a, const Peer& b) {
                return a.ip == b.ip && a.port == b.port;
            });
            all_peers.erase(last, all_peers.end());
        }

        std::cout << "Total unique peers: " << all_peers.size() << std::endl;

        size_t saved_count = pieces.PiecesSavedToDiscCount();
        size_t total_count = pieces.TotalPiecesCount();
        auto missing = pieces.GetMissingPieces();
        std::cout << "Progress: " << saved_count << "/" << total_count
                  << " (missing: " << missing.size() << " pieces)" << std::endl;

        if (!missing.empty() && missing.size() <= 10) {
            std::cout << "Missing pieces: ";
            for (size_t piece : missing) {
                std::cout << piece << " ";
            }
            std::cout << std::endl;
        }

        if (all_peers.empty()) {
            std::cout << "No peers, waiting 30s..." << std::endl;
            std::this_thread::sleep_for(30s);
            continue;
        }

        if (pieces.QueueIsEmpty() && !pieces.IsDownloadComplete()) {
            std::cout << "Queue is empty but download not complete. Forcing requeue of missing pieces..." << std::endl;
            pieces.ForceRequeueMissingPieces();
            retry_count++;
        }

        TorrentTracker combined_tracker(trackers[0]);
        combined_tracker.SetPeers(all_peers);

        RunDownloadMultithread(pieces, torrent_file, combined_tracker);

        if (!pieces.IsDownloadComplete()) {
            if (retry_count >= max_retries) {
                std::cout << "Max retries reached. Giving up on remaining pieces." << std::endl;
                break;
            }

            std::cout << "Still missing " << missing.size() << " pieces (retry "
                      << retry_count << "/" << max_retries << ")" << std::endl;
            std::cout << "Waiting 30s before next tracker cycle..." << std::endl;
            std::this_thread::sleep_for(30s);
        }
    }

    if (pieces.IsDownloadComplete()) {
        std::cout << "DOWNLOAD COMPLETED SUCCESSFULLY!" << std::endl;
    } else {
        auto missing = pieces.GetMissingPieces();
        std::cout << "DOWNLOAD INCOMPLETE! Missing " << missing.size() << " pieces." << std::endl;
        if (!missing.empty()) {
            std::cout << "Final missing pieces: ";
            for (size_t piece : missing) {
                std::cout << piece << " ";
            }
            std::cout << std::endl;
        }
    }
}

void TorrentClient::DownloadTorrent(const std::filesystem::path& torrent_file_path,
                                   const std::filesystem::path& output_directory) {
    is_terminated = false;

    TorrentFile torrentFile = LoadTorrentFile(torrent_file_path);

    std::cout << "Downloading " << torrentFile.piece_hashes.size() << " pieces" << std::endl;
    std::cout << "File: " << torrentFile.name << " (" << torrentFile.length << " bytes)" << std::endl;
    std::cout << "Peer ID: " << peer_id << std::endl;

    PieceStorage pieces(torrentFile, output_directory);

    auto start_time = std::chrono::steady_clock::now();
    DownloadFromTracker(torrentFile, pieces);
    auto end_time = std::chrono::steady_clock::now();

    pieces.CloseOutputFile();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);

    size_t saved_count = pieces.PiecesSavedToDiscCount();
    size_t total_count = torrentFile.piece_hashes.size();
    bool is_complete = pieces.IsDownloadComplete();

    std::cout << "=== TORRENT DOWNLOAD FINISHED ===" << std::endl;
    std::cout << "Download time: " << duration.count() << " seconds" << std::endl;
    std::cout << "Saved " << saved_count << "/" << total_count << " pieces to disk" << std::endl;
    std::cout << "All pieces complete and valid: " << (is_complete ? "YES" : "NO") << std::endl;
    std::cout << "Completion: " << (saved_count * 100 / total_count) << "%" << std::endl;

    if (is_complete) {
        std::cout << "Download completed successfully!" << std::endl;
    } else {
        std::cout << "Download interrupted! Missing or invalid: "
                  << (total_count - saved_count) << " pieces." << std::endl;
        pieces.PrintMissingPieces();
        pieces.PrintDownloadStatus();
    }
}

void TorrentClient::AddLogMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char time_string[20]; // have to use char[]
    std::strftime(time_string, sizeof(time_string), "[%H:%M:%S]", std::localtime(&time));
    
    log_messages.push_back(std::string(time_string) + " " + message);
    
    if (log_messages.size() > 1000) {
        log_messages.erase(log_messages.begin(), log_messages.begin() + 500);
    }
}

TorrentTask TorrentClient::GetCurrentTask() const {
    std::lock_guard<std::mutex> lock(task_mutex);
    return current_task;
}

std::vector<std::string> TorrentClient::GetLogMessages(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    if (log_messages.size() <= maxCount) {
        return log_messages;
    }
    
    return std::vector<std::string>(
        log_messages.end() - maxCount,
        log_messages.end()
    );
}

void TorrentClient::UpdateTaskStatus(TorrentStatus status) {
    std::lock_guard<std::mutex> lock(task_mutex);
    current_task.status = status;
    current_task.last_update = std::chrono::system_clock::now();
}

void TorrentClient::UpdateTaskFromPieceStorage(const PieceStorage& storage) {
    std::lock_guard<std::mutex> lock(task_mutex);
    size_t new_piece_length;
    if (current_task.total_pieces_count > 0) {
        new_piece_length = current_task.total_size / current_task.total_pieces_count;
    } else {
        new_piece_length = 0;
    }
    current_task.UpdateFromPieceStorage(storage, new_piece_length);
    current_task.last_update = std::chrono::system_clock::now();
}

void TorrentClient::UpdateTaskFromTracker(const TorrentTracker& tracker) {
    std::lock_guard<std::mutex> lock(task_mutex);
    current_task.total_peers_count = tracker.GetPeers().size();
    current_task.last_update = std::chrono::system_clock::now();
}

void TorrentClient::PauseDownload() {
    is_paused = true;
    UpdateTaskStatus(TorrentStatus::kPaused);
    AddLogMessage("Download paused");
}

void TorrentClient::ResumeDownload() {
    is_paused = false;
    UpdateTaskStatus(TorrentStatus::kDownloading);
    AddLogMessage("Download resumed");
}

bool TorrentClient::IsDownloading() const {
    return current_task.status == TorrentStatus::kDownloading;
}

bool TorrentClient::IsPaused() const {
    return is_paused.load();
}
