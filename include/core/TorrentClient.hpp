#pragma once

#include "core/TorrentFile.hpp"
#include "core/TorrentTracker.hpp"
#include "core/PieceStorage.hpp"
#include <filesystem>
#include <atomic>

class TorrentClient {
public:
    TorrentClient(const std::string& peerId = "TESTAPPDONTWORRY");

    void DownloadTorrent(const std::filesystem::path& torrentFilePath,
                        const std::filesystem::path& outputDirectory);

    const std::string& GetPeerId() const { return peer_id; }
    void SetPeerId(const std::string& peerId) { peer_id = peerId; }

private:
    std::string peer_id;
    std::atomic<bool> is_terminated = false;

    std::string GenerateRandomSuffix(size_t length = 4);
    bool RunDownloadMultithread(PieceStorage& pieces, const TorrentFile& torrent_file,
                               const TorrentTracker& tracker);
    void DownloadFromTracker(const TorrentFile& torrentFile, PieceStorage& pieces);
};
