#include "core/TorrentClient.hpp"
#include "net/PeerConnect.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <algorithm>


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
    using namespace std::chrono_literals;
    UpdateTaskStatus(TorrentStatus::kDownloading);
    UpdateTaskFromTracker(tracker);
    AddLogMessage("Starting download with " + std::to_string(tracker.GetPeers().size()) + " peers");
    
    std::vector<std::thread> peer_threads;
    std::vector<std::shared_ptr<PeerConnect>> peer_connections;

    for (const Peer& peer : tracker.GetPeers()) {
        try {
            peer_connections.emplace_back(
                std::make_shared<PeerConnect>(peer, torrent_file, peer_id, pieces)
            );
        } catch (const std::exception& e) {
            std::string error_msg = "Failed to connect to " + peer.ip + ":" + std::to_string(peer.port) + " - " + e.what();
            AddLogMessage(error_msg);
        }
    }

    if (peer_connections.empty()) {
        AddLogMessage("No valid peer connections established");
        return true;
    }

    peer_threads.reserve(peer_connections.size());
    for (auto& peer_connect_ptr : peer_connections) {
        peer_threads.emplace_back([peer_connect_ptr]() {
            while (!peer_connect_ptr->IsTerminated()) {
                try {
                    peer_connect_ptr->Run();
                } catch (const std::exception& e) {
                    if (!peer_connect_ptr->IsTerminated()) {
                        std::this_thread::sleep_for(5s);
                    }
                }
            }
        });
    }

    AddLogMessage("Started " + std::to_string(peer_threads.size()) + " peer threads");

    const size_t target_pieces = pieces.TotalPiecesCount();
    
    {
        std::lock_guard<std::mutex> lock(task_mutex);
        current_task.total_pieces_count = target_pieces;
        current_task.total_size = torrent_file.length;
    }
    
    AddLogMessage("Downloading " + std::to_string(target_pieces) + " pieces");

    bool endgame_mode = false;
    auto last_requeue_time = std::chrono::steady_clock::now();
    const auto requeue_interval = std::chrono::seconds(10);
    auto last_status_update = std::chrono::steady_clock::now();

    while (!is_terminated && !pieces.IsDownloadComplete()) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_status_update > 500ms) {
            UpdateTaskFromPieceStorage(pieces);
            last_status_update = now;
        }
        
        if (is_paused) {
            std::this_thread::sleep_for(100ms);
            continue;
        }

        size_t missing_count = pieces.GetMissingPieces().size();

        if (!endgame_mode && missing_count <= 10) {
            endgame_mode = true;
            AddLogMessage("Entering endgame mode - " + std::to_string(missing_count) + " pieces remaining");
        }

        if (endgame_mode && pieces.QueueIsEmpty()) {
            if (now - last_requeue_time > requeue_interval) {
                AddLogMessage("Endgame: requeuing " + std::to_string(missing_count) + " missing pieces");
                pieces.ForceRequeueMissingPieces();
                last_requeue_time = now;
            }
        }

        if (!pieces.HasActiveWork()) {
            std::this_thread::sleep_for(500ms);
        } else {
            std::this_thread::sleep_for(100ms);
        }
    }

    UpdateTaskFromPieceStorage(pieces);
    
    if (pieces.IsDownloadComplete()) {
        UpdateTaskStatus(TorrentStatus::kCompleted);
        AddLogMessage("Download completed successfully");
    } else {
        UpdateTaskStatus(TorrentStatus::kError);
        AddLogMessage("Download incomplete - missing pieces");
    }

    is_terminated = true;

    for (auto& peer_connect_ptr : peer_connections) {
        peer_connect_ptr->Terminate();
    }

    for (auto& thread : peer_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    return !pieces.IsDownloadComplete();
}

void TorrentClient::DownloadFromTracker(const TorrentFile& torrent_file, PieceStorage& pieces) {
    using namespace std::chrono_literals;
    UpdateTaskStatus(TorrentStatus::kConnected);
    AddLogMessage("Connecting to trackers...");
    
    std::vector<std::string> trackers = {
        torrent_file.announce,
        "udp://tracker.opentrackr.org:1337/announce",
        "udp://open.stealth.si:80/announce",
        "udp://exodus.desync.com:6969/announce",
        "udp://tracker.torrent.eu.org:451/announce"
    };

    std::sort(trackers.begin(), trackers.end());
    trackers.erase(std::unique(trackers.begin(), trackers.end()), trackers.end());

    AddLogMessage("Using " + std::to_string(trackers.size()) + " trackers");

    int retry_count = 0;
    const int max_retries = 10;
    auto last_tracker_update = std::chrono::steady_clock::now();

    while (!is_terminated && !pieces.IsDownloadComplete()) {
        if (std::chrono::steady_clock::now() - last_tracker_update > 1s) {
            UpdateTaskFromPieceStorage(pieces);
            last_tracker_update = std::chrono::steady_clock::now();
        }
        
        if (is_paused) {
            std::this_thread::sleep_for(100ms);
            continue;
        }

        std::vector<Peer> all_peers;

        for (size_t i = 0; i < trackers.size() && !is_terminated; ++i) {
            try {
                TorrentTracker tracker(trackers[i]);
                tracker.UpdatePeers(torrent_file, peer_id, 12345);
                const auto& peers = tracker.GetPeers();
                all_peers.insert(all_peers.end(), peers.begin(), peers.end());
                AddLogMessage(trackers[i] + " - " + std::to_string(peers.size()) + " peers");
            } catch (const std::exception& e) {
                AddLogMessage("Tracker " + trackers[i] + " error: " + e.what());
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

        {
            std::lock_guard<std::mutex> lock(task_mutex);
            current_task.total_peers_count = all_peers.size();
        }

        AddLogMessage("Total unique peers: " + std::to_string(all_peers.size()));

        if (all_peers.empty()) {
            AddLogMessage("No peers found, waiting 30 seconds...");
            std::this_thread::sleep_for(30s);
            continue;
        }

        if (pieces.QueueIsEmpty() && !pieces.IsDownloadComplete()) {
            AddLogMessage("Queue empty, requeuing missing pieces");
            pieces.ForceRequeueMissingPieces();
            retry_count++;
        }

        TorrentTracker combined_tracker(trackers[0]);
        combined_tracker.SetPeers(all_peers);

        RunDownloadMultithread(pieces, torrent_file, combined_tracker);

        if (!pieces.IsDownloadComplete()) {
            if (retry_count >= max_retries) {
                AddLogMessage("Max retries reached, stopping download");
                break;
            }

            AddLogMessage("Retry " + std::to_string(retry_count) + "/" + std::to_string(max_retries) + 
                         " - " + std::to_string(pieces.GetMissingPieces().size()) + " pieces remaining");
            std::this_thread::sleep_for(30s);
        }
    }

    UpdateTaskFromPieceStorage(pieces);
    
    if (pieces.IsDownloadComplete()) {
        UpdateTaskStatus(TorrentStatus::kCompleted);
        AddLogMessage("Download completed successfully!");
    } else {
        UpdateTaskStatus(TorrentStatus::kError);
        AddLogMessage("Download incomplete");
    }
}

void TorrentClient::DownloadTorrent(const std::filesystem::path& torrent_file_path,
                                   const std::filesystem::path& output_directory) {
    is_terminated = false;
    is_paused = false;

    UpdateTaskStatus(TorrentStatus::kLoading);
    AddLogMessage("Loading torrent file: " + torrent_file_path.string());

    TorrentFile torrentFile = LoadTorrentFile(torrent_file_path);

    {
        std::lock_guard<std::mutex> lock(task_mutex);
        current_task.filename = torrentFile.name;
        current_task.total_size = torrentFile.length;
        current_task.info_hash = torrentFile.info_hash;
        current_task.announce_url = torrentFile.announce;
        current_task.output_file_path = output_directory.string();
        current_task.total_pieces_count = torrentFile.piece_hashes.size();
        current_task.start_time = std::chrono::system_clock::now();
        current_task.last_update = std::chrono::system_clock::now();
    }
    
    AddLogMessage("File: " + torrentFile.name + " (" + 
                  std::to_string(torrentFile.length) + " bytes, " +
                  std::to_string(torrentFile.piece_hashes.size()) + " pieces)");

    PieceStorage pieces(torrentFile, output_directory);

    auto start_time = std::chrono::steady_clock::now();
    
    try {
        DownloadFromTracker(torrentFile, pieces);
    } catch (const std::exception& e) {
        UpdateTaskStatus(TorrentStatus::kError);
        AddLogMessage("Download error: " + std::string(e.what()));
    }
    
    auto end_time = std::chrono::steady_clock::now();

    pieces.CloseOutputFile();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    AddLogMessage("Download finished in " + std::to_string(duration.count()) + " seconds");
    
    if (pieces.IsDownloadComplete()) {
        AddLogMessage("Download completed successfully!");
    } else {
        AddLogMessage("Download incomplete!");
    }
}

void TorrentClient::AddLogMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char time_string[20];
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
