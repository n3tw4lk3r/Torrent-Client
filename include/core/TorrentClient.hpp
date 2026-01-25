#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <vector>

#include "core/HttpTracker.hpp"
#include "core/PieceStorage.hpp"
#include "core/TorrentFile.hpp"
#include "core/TorrentTask.hpp"
#include "net/PeerConnection.hpp"

class TorrentClient {
public:
    explicit TorrentClient(const std::string& peer_id = "TESTAPPDONTWORRY");

    void DownloadTorrent(const std::filesystem::path& torrent_file_path,
                         const std::filesystem::path& output_directory);

    const std::string& GetPeerId() const { return peer_id; }
    void SetPeerId(const std::string& new_peer_id) { peer_id = new_peer_id; }

    TorrentTask GetCurrentTask() const;
    std::vector<std::string> GetLogMessages(size_t max_count = 50) const;
    void PauseDownload();
    void ResumeDownload();
    bool IsDownloading() const;
    bool IsPaused() const;

private:
    std::string peer_id;
    std::atomic<bool> is_terminated{false};
    std::atomic<bool> is_paused{false};

    mutable std::mutex task_mutex;
    TorrentTask current_task;
    mutable std::mutex log_mutex;
    std::vector<std::string> log_messages;
    std::vector<std::shared_ptr<PeerConnection>> peer_connections;

    void AddLogMessage(const std::string& message);
    void UpdateTaskStatus(TorrentStatus status);
    void UpdateTaskFromPieceStorage(const PieceStorage& storage);
    void UpdateTaskFromTracker(const HttpTracker& tracker);

    std::string GenerateRandomSuffix(size_t length = 4);
    bool RunDownloadMultithread(PieceStorage& pieces, 
                                const TorrentFile& torrent_file,
                                const HttpTracker& tracker);
    void DownloadFromTracker(const TorrentFile& torrent_file, 
                             PieceStorage& pieces);
};
