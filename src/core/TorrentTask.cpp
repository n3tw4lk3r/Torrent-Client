#include "core/TorrentTask.hpp"
#include "core/PieceStorage.hpp"
#include <sstream>
#include <iomanip>

std::string TorrentTask::FormatBytes(uint64_t bytes) const {
    const std::vector<std::string> units = { "B", "KB", "MB", "GB", "TB" };
    size_t unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

std::string TorrentTask::GetFormattedSize() const {
    return FormatBytes(total_size);
}

std::string TorrentTask::GetFormattedDownloaded() const {
    return FormatBytes(downloaded);
}

std::string TorrentTask::GetFormattedSpeed() const {
    if (download_speed == 0)
        return "0 B/s";
    return FormatBytes(download_speed) + "/s";
}

std::string TorrentTask::GetFormattedProgress() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << progress << "%";
    return oss.str();
}

std::string TorrentTask::GetStatusString() const {
    switch (status) {
        default:
        case TorrentStatus::kNoTorrent:
            return "No Torrent";
        case TorrentStatus::kLoading:
            return "Loading";
        case TorrentStatus::kDownloading:
            return "Downloading";
        case TorrentStatus::kPaused:
            return "Paused";
        case TorrentStatus::kCompleted:
            return "Completed";
        case TorrentStatus::kError:
            return "Error";
        case TorrentStatus::kConnected:
            return "Connecting";
    }
}

std::string TorrentTask::GetPeersString() const {
    std::ostringstream oss;
    oss << connected_peers << "/" << total_peers_count;
    return oss.str();
}

void TorrentTask::UpdateFromPieceStorage(const PieceStorage& storage, 
                                        size_t defaultPieceLength) {
    total_pieces_count = storage.TotalPiecesCount();
    downloaded_pieces_count = storage.PiecesSavedToDiscCount();
    missing_pieces = storage.GetMissingPieces();
    
    if (total_pieces_count > 0) {
        progress = (static_cast<double>(downloaded_pieces_count) / total_pieces_count) * 100.0;
        downloaded = downloaded_pieces_count * defaultPieceLength;
        
        if (downloaded > total_size && total_size > 0) {
            downloaded = total_size;
        }
    }
}
