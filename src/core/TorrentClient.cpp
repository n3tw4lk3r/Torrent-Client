#include "core/TorrentClient.hpp"
#include "net/PeerConnect.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <algorithm>
#include <set>

using namespace std::chrono_literals;

TorrentClient::TorrentClient(const std::string& peer_id)
    : peer_id(peer_id + GenerateRandomSuffix()) {}

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
            int attempts = 0;
            static constexpr int max_number_of_attempts = 5;
            bool should_try_again = true;

            while (should_try_again && attempts < max_number_of_attempts) {
                try {
                    ++attempts;
                    peer_connect_ptr->Run();
                    should_try_again = false; // Success
                } catch (const std::exception& e) {
                    std::cerr << "Connection attempt " << attempts << " failed: "
                              << e.what() << std::endl;
                    should_try_again = peer_connect_ptr->Failed() && attempts < max_number_of_attempts;
                    if (should_try_again) {
                        std::this_thread::sleep_for(2s * attempts);
                    }
                }
            }
        });
    }

    std::cout << "Started " << peer_threads.size() << " threads for peers" << std::endl;

    const size_t target_pieces = pieces.TotalPiecesCount();

    std::cout << "=== DOWNLOAD STARTED ===" << std::endl;
    std::cout << "Target pieces: " << target_pieces << std::endl;
    std::cout << "Initial saved pieces: " << pieces.PiecesSavedToDiscCount() << std::endl;

    while (!is_terminated && !pieces.IsDownloadComplete()) {
        if (!pieces.HasActiveWork()) {
            auto missing = pieces.GetMissingPieces();
            std::cout << "No active work. Missing: " << missing.size()
                      << ", Queue: " << (pieces.QueueIsEmpty() ? "empty" : "has work")
                      << std::endl;

            std::this_thread::sleep_for(500ms);
        } else {
            std::this_thread::sleep_for(1000ms);
        }

        size_t current_saved_count = pieces.PiecesSavedToDiscCount();
        if (current_saved_count % 5 == 0 || current_saved_count == target_pieces) {
            std::cout << "Progress: " << current_saved_count << "/" << target_pieces << std::endl;
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
        }
    }

    for (auto& thread : peer_threads) {
        if (thread.joinable()) {
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

    while (!is_terminated && !pieces.IsDownloadComplete()) {
        std::vector<Peer> all_peers;

        for (size_t i = 0; i < trackers.size() && !is_terminated; ++i) {
            try {
                TorrentTracker tracker(trackers[i]);
                tracker.UpdatePeers(torrent_file, peer_id, 12345);
                const auto& peers = tracker.GetPeers();
                all_peers.insert(all_peers.end(), peers.begin(), peers.end());
                std::cout << "✓ " << trackers[i] << " - " << peers.size() << " peers" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "✗ " << trackers[i] << " - " << e.what() << std::endl;
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
        std::cout << "Progress: " << pieces.PiecesSavedToDiscCount() << "/" << pieces.TotalPiecesCount() << std::endl;

        if (all_peers.empty()) {
            std::cout << "No peers, waiting 30s..." << std::endl;
            std::this_thread::sleep_for(30s);
            continue;
        }

        TorrentTracker combined_tracker(trackers[0]);
        combined_tracker.SetPeers(all_peers);
        RunDownloadMultithread(pieces, torrent_file, combined_tracker);

        if (!pieces.IsDownloadComplete()) {
            auto missing = pieces.GetMissingPieces();
            std::cout << "Still missing " << missing.size() << " pieces" << std::endl;
            std::cout << "Waiting 60s before next tracker cycle..." << std::endl;
            std::this_thread::sleep_for(60s);
        }
    }

    if (pieces.IsDownloadComplete()) {
        std::cout << "DOWNLOAD COMPLETED SUCCESSFULLY!" << std::endl;
    } else {
        std::cout << "DOWNLOAD TERMINATED BEFORE COMPLETION" << std::endl;
    }
}

void TorrentClient::DownloadTorrent(const std::filesystem::path& torrent_file_path,
                                   const std::filesystem::path& output_directory) {
    is_terminated = false;

    TorrentFile torrentFile = LoadTorrentFile(torrent_file_path);

    std::cout << "Downloading " << torrentFile.piece_hashes.size() << " pieces" << std::endl;
    std::cout << "File: " << torrentFile.name << " (" << torrentFile.length << " bytes)" << std::endl;
    std::cout << "Peer ID: " << peer_id << std::endl;

    PieceStorage pieces(torrentFile, output_directory, torrentFile.piece_hashes.size());

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
