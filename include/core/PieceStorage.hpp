#pragma once

#include "core/Piece.hpp"
#include "core/TorrentFile.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <queue>
#include <mutex>
#include <vector>

class PieceStorage {
public:
    PieceStorage(const TorrentFile& torrent_file,
                 const std::filesystem::path& output_directory,
                 int piecesToDownloadCnt = -1);

    PiecePtr GetNextPieceToDownload();
    void PieceProcessed(const PiecePtr& piece);
    void Enqueue(const PiecePtr& piece);
    bool QueueIsEmpty() const;

    size_t TotalPiecesCount() const;
    size_t PiecesSavedToDiscCount() const;
    size_t getToDownloadCnt() const;

    void CloseOutputFile();
    void PrintMissingPieces() const;
    bool IsDownloadComplete() const;
    bool HasActiveWork() const;
    std::vector<size_t> GetMissingPieces() const;
    void ForceRequeueMissingPieces();

    bool HasActivePieces() const {
        return active_pieces_ > 0 || !QueueIsEmpty();
    }


    bool HasActiveDownloads() const;
    size_t PiecesCompleteCount() const;
    void ForceRequeueAllMissingPieces();
    void PrintDownloadStatus() const;
    void PrintDetailedStatus() const;

private:
    void SavePieceToDisk(const PiecePtr& piece);

    std::queue<PiecePtr> remaining_pieces_queue;
    mutable std::mutex queue_mutex;
    std::ofstream file;
    std::atomic<int> active_pieces_{0};
    mutable std::mutex file_mutex;
    std::vector<size_t> indices_of_pieces_saved_to_disk;

    size_t default_piece_length;
    size_t total_piece_count;
    int number_of_pieces_to_download;
    TorrentFile torrent_file;
};
