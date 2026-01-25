#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

enum class TorrentStatus {
    kNoTorrent,
    kLoading,
    kDownloading,
    kPaused,
    kCompleted,
    kError,
    kConnected
};

struct TorrentTask {
    std::string filename;
    TorrentStatus status;
    double progress;
    
    uint64_t total_size;
    uint64_t downloaded;
    uint64_t uploaded;
    
    int connected_peers;
    int total_peers_count;
    
    std::string info_hash;
    std::string announce_url;
    std::string output_file_path;
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_update;
    
    std::vector<size_t> missing_pieces;
    size_t total_pieces_count;
    size_t downloaded_pieces_count;
    
    TorrentTask() : status(TorrentStatus::kNoTorrent),
                    progress(0.0),
                    total_size(0),
                    downloaded(0),
                    uploaded(0),
                    connected_peers(0),
                    total_peers_count(0),
                    total_pieces_count(0),
                    downloaded_pieces_count(0) {}
    
    void SetConnectedPeers(int new_count);
    std::string GetFormattedSize() const;
    std::string GetFormattedDownloaded() const;
    std::string GetFormattedProgress() const;
    std::string GetStatusString() const;
    std::string GetPeersString() const;
    std::string FormatBytes(uint64_t bytes) const;
    void UpdateFromPieceStorage(const class PieceStorage& storage, 
                                size_t default_piece_length);
};
