#include "core/TorrentTask.hpp"

#include <iomanip>
#include <sstream>

#include "core/PieceStorage.hpp"

std::string TorrentTask::FormatBytes(uint64_t bytes) const {
    const std::vector<std::string> units = { "B", "KB", "MB", "GB", "TB" };
    size_t unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < units.size() - 1) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

void TorrentTask::SetConnectedPeers(int new_count) {
    connected_peers = new_count;
}

std::string TorrentTask::GetFormattedSize() const {
    return FormatBytes(total_size);
}

std::string TorrentTask::GetFormattedDownloaded() const {
    return FormatBytes(downloaded);
}

std::string TorrentTask::GetFormattedProgress() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << progress << "%";
    return oss.str();
}

std::string TorrentTask::GetStatusString() const {
    switch (status) {
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
        default:
            return "Unknown";
    }
}

std::string TorrentTask::GetPeersString() const {
    std::ostringstream oss;
    oss << connected_peers << "/" << total_peers_count;
    return oss.str();
}

void TorrentTask::UpdateFromPieceStorage(const PieceStorage& storage, 
                                         size_t default_piece_length) {
    total_pieces_count = storage.TotalPiecesCount();
    downloaded_pieces_count = storage.PiecesSavedToDiscCount();
    missing_pieces = storage.GetMissingPieces();
    
    if (total_pieces_count > 0) {
        progress = (static_cast<double>(downloaded_pieces_count) / total_pieces_count) * 100.0;
        downloaded = downloaded_pieces_count * default_piece_length;
        
        if (downloaded > total_size && total_size > 0) {
            downloaded = total_size;
        }
    }
}
