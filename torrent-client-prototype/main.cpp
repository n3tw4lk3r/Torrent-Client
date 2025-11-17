#include "torrent_tracker.h"
#include "piece_storage.h"
#include "peer_connect.h"
#include "byte_tools.h"

#include <cassert>
#include <iostream>
#include <filesystem>
#include <random>
#include <thread>

std::string RandomString(size_t length) {
    std::random_device random;
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(random() % ('Z' - 'A') + 'A');
    }
    return result;
}

std::mutex cerrMutex, coutMutex;
const std::string PeerId = "TESTAPPDONTWORRY" + RandomString(4);

bool RunDownloadMultithread(PieceStorage& pieces, const TorrentFile& torrentFile, const std::string& ourId, const TorrentTracker& tracker) {
    using namespace std::chrono_literals;

    std::vector<std::thread> peerThreads;
    std::vector<std::shared_ptr<PeerConnect>> peerConnections;

    for (const Peer& peer : tracker.GetPeers()) {
        peerConnections.emplace_back(std::make_shared<PeerConnect>(peer, torrentFile, ourId, pieces));
    }

    peerThreads.reserve(peerConnections.size());

    for (auto& peerConnectPtr : peerConnections) {
        peerThreads.emplace_back(
                [peerConnectPtr] () {
                    bool tryAgain = true;
                    int attempts = 0;
                    do {
                        try {
                            ++attempts;
                            peerConnectPtr->Run();
                        } catch (const std::runtime_error& e) {
                            std::cerr << "Runtime error: " << e.what() << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << "Exception: " << e.what() << std::endl;
                        } catch (...) {
                            std::cerr << "Unknown error" << std::endl;
                        }
                        tryAgain = peerConnectPtr->Failed() && attempts < 3;
                    } while (tryAgain);
                }
        );
    }

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Started " << peerThreads.size() << " threads for peers" << std::endl;
    }

    std::this_thread::sleep_for(10s);
    while (pieces.PiecesSavedToDiscCount() != pieces.getToDownloadCnt()) {
        if (pieces.PiecesInProgressCount() == 0) {
            {
                std::lock_guard<std::mutex> coutLock(coutMutex);
                std::cout
                        << "Want to download more pieces but all peer connections are not working. Let's request new peers"
                        << std::endl;
            }

            for (auto& peerConnectPtr : peerConnections) {
                peerConnectPtr->Terminate();
            }
            for (std::thread& thread : peerThreads) {
                thread.join();
            }
            return true;
        }
        std::this_thread::sleep_for(1s);
    }

    {
        std::lock_guard<std::mutex> coutLock(coutMutex);
        std::cout << "Terminating all peer connections" << std::endl;
    }
    for (auto& peerConnectPtr : peerConnections) {
        peerConnectPtr->Terminate();
    }

    std::this_thread::sleep_for(5s);
    for (std::thread& thread : peerThreads) {




        throw std::runtime_error("no error");
        // thread.join();
    }

    return false;
}

void DownloadTorrentFile(const TorrentFile& torrentFile, PieceStorage& pieces, const std::string& ourId) {
    std::cout << "Connecting to tracker " << torrentFile.announce << std::endl;
    TorrentTracker tracker(torrentFile.announce);
    bool requestMorePeers = false;
    do {
        tracker.UpdatePeers(torrentFile, ourId, 12345);

        if (tracker.GetPeers().empty()) {
            std::cerr << "No peers found. Cannot download a file" << std::endl;
        }

        std::cout << "Found " << tracker.GetPeers().size() << " peers" << std::endl;
        for (const Peer& peer : tracker.GetPeers()) {
            std::cout << "Found peer " << peer.ip << ":" << peer.port << std::endl;
        }

        requestMorePeers = RunDownloadMultithread(pieces, torrentFile, ourId, tracker);
    } while (requestMorePeers);
}

void run(std::filesystem::path torrentFilePath, std::filesystem::path outputFilePath, int percentage) {
    TorrentFile torrentFile = LoadTorrentFile(torrentFilePath);
    int pieceCount = torrentFile.pieceHashes.size();
    int toDownloadCount = pieceCount * percentage / 100; // like in python checker
    std::cout << "Downloading " << toDownloadCount << " pieces." << std::endl;
    PieceStorage pieces(torrentFile, outputFilePath, toDownloadCount);
    DownloadTorrentFile(torrentFile, pieces, PeerId);
}

int main(int argc, char* argv[]) {
    assert(argc >= 6);

    std::string outputFilename, tfName;
    int percentage;
    for (int i = 0; i < argc; ++i) {
        // ideally should validate args
        std::string arg = argv[i];
        if (arg == "-d") {
            outputFilename = argv[i + 1];
        }
            
        if (arg == "-p") {
            percentage = std::stoi(argv[i + 1]);
        }

    }
    tfName = argv[argc - 1];
    std::cout << "out: " << outputFilename << " tf: " << tfName << " p: " << percentage << std::endl;
    std::filesystem::path outputPath(outputFilename), tfPath(tfName); 
    run(tfPath, outputPath, percentage);
}