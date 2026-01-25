#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <vector>

#include "core/Piece.hpp"
#include "core/TorrentFile.hpp"

class PieceStorage {
public:
    PieceStorage(const TorrentFile& torrent_file,
                 const std::filesystem::path& output_directory);

    PiecePtr GetNextPieceToDownload();
    void PieceProcessed(const PiecePtr& piece);
    void Enqueue(const PiecePtr& piece);
    bool QueueIsEmpty() const;
    bool IsPieceAlreadySaved(size_t piece_index) const;
    size_t TotalPiecesCount() const;
    size_t PiecesSavedToDiscCount() const;

    void CloseOutputFile();
    bool IsDownloadComplete() const;
    bool HasActiveWork() const;
    std::vector<size_t> GetMissingPieces() const;
    void ForceRequeueMissingPieces();

private:
    void SavePieceToDisk(const PiecePtr& piece);
    void InitializeOutputFile();

    std::queue<PiecePtr> remaining_pieces_queue;
    mutable std::mutex queue_mutex;

    std::ofstream file;
    mutable std::mutex file_mutex;

    std::unordered_set<size_t> saved_pieces;

    std::filesystem::path output_directory;
    size_t default_piece_length;
    size_t total_piece_count;
    TorrentFile torrent_file;
};
