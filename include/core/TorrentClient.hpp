#pragma once

#include "core/TorrentFile.hpp"
#include "core/TorrentTracker.hpp"
#include "core/PieceStorage.hpp"
#include "core/TorrentTask.hpp"
#include <filesystem>
#include <atomic>
#include <mutex>
#include <vector>

class TorrentClient {
public:
    TorrentClient(const std::string& peerId = "TESTAPPDONTWORRY");

    void DownloadTorrent(const std::filesystem::path& torrentFilePath,
                        const std::filesystem::path& outputDirectory);

    const std::string& GetPeerId() const { return peer_id; }
    void SetPeerId(const std::string& peerId) { peer_id = peerId; }

    TorrentTask GetCurrentTask() const;
    std::vector<std::string> GetLogMessages(size_t maxCount = 50) const;
    void PauseDownload();
    void ResumeDownload();
    bool IsDownloading() const;
    bool IsPaused() const;

private:
    std::string peer_id;
    std::atomic<bool> is_terminated = false;
    std::atomic<bool> is_paused = false;

    mutable std::mutex task_mutex;
    TorrentTask current_task;
    mutable std::mutex log_mutex;
    std::vector<std::string> log_messages;

    void AddLogMessage(const std::string& message);
    void UpdateTaskStatus(TorrentStatus status);
    void UpdateTaskFromPieceStorage(const PieceStorage& storage);
    void UpdateTaskFromTracker(const TorrentTracker& tracker);

    std::string GenerateRandomSuffix(size_t length = 4);
    bool RunDownloadMultithread(PieceStorage& pieces, const TorrentFile& torrent_file,
                               const TorrentTracker& tracker);
    void DownloadFromTracker(const TorrentFile& torrentFile, PieceStorage& pieces);
};
