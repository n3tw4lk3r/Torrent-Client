#include "core/PieceStorage.hpp"
#include "core/Piece.hpp"
#include <iostream>
#include <algorithm>

PieceStorage::PieceStorage(const TorrentFile& torrent_file, const std::filesystem::path& output_directory,
                          int piecesToDownloadCnt)
    : default_piece_length(torrent_file.piece_length)
    , number_of_pieces_to_download(piecesToDownloadCnt)
    , torrent_file(torrent_file) {

    total_piece_count = torrent_file.piece_hashes.size();

    std::cout << "=== PIECE STORAGE INIT ===" << std::endl;
    std::cout << "Total pieces: " << total_piece_count << std::endl;
    std::cout << "Piece length: " << torrent_file.piece_length << std::endl;
    std::cout << "Total length: " << torrent_file.length << std::endl;
    std::cout << "Last piece size: " << (torrent_file.length % torrent_file.piece_length) << std::endl;

    for (size_t i = 0; i < total_piece_count; ++i) {
        size_t pieceLength;
        if (i == total_piece_count - 1) {
            pieceLength = torrent_file.length % torrent_file.piece_length;
            if (pieceLength == 0) {
                pieceLength = torrent_file.piece_length;
            }
        } else {
            pieceLength = torrent_file.piece_length;
        }

        if (pieceLength == 0) {
            std::cerr << "ERROR: Piece " << i << " has zero length!" << std::endl;
            pieceLength = torrent_file.piece_length;
        }

        auto piece = std::make_shared<Piece>(i, pieceLength, torrent_file.piece_hashes[i]);
        remaining_pieces_queue.push(piece);
    }

    std::string filename = (output_directory / torrent_file.name).generic_string();
    file.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file: " + filename);
    }

    const size_t chunk_size = 100 * (1 << 20);
    for (size_t offset = 0; offset < torrent_file.length; offset += chunk_size) {
        size_t write_size = std::min(chunk_size, torrent_file.length - offset);
        file.seekp(offset);
        std::vector<char> buffer(write_size, 0);
        file.write(buffer.data(), write_size);
    }
    file.flush();

    std::cout << "Created output file: " << filename << " (" << torrent_file.length << " bytes)" << std::endl;
    std::cout << "Initialized " << total_piece_count << " pieces in storage" << std::endl;
}
bool PieceStorage::HasActiveWork() const {
    return active_pieces_ > 0 || !QueueIsEmpty();
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::lock_guard<std::mutex> lock(queue_mutex);

    if (remaining_pieces_queue.empty()) {
        return nullptr;
    }

    PiecePtr piece = remaining_pieces_queue.front();
    remaining_pieces_queue.pop();
    ++active_pieces_;
    return piece;
}

void PieceStorage::Enqueue(const PiecePtr& piece) {
    if (!piece) return;

    std::lock_guard<std::mutex> lock(queue_mutex);
    piece->Reset();
    remaining_pieces_queue.push(piece);
    --active_pieces_;
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    if (!piece) return;

    if (!piece->HashMatches()) {
        std::cout << "Piece " << piece->GetIndex() << " hash mismatch, requeuing..." << std::endl;
        Enqueue(piece);
        return;
    }

    SavePieceToDisk(piece);
    --active_pieces_;
}

bool PieceStorage::QueueIsEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return remaining_pieces_queue.empty();
}
bool PieceStorage::HasActiveDownloads() const {
    return false;
}

size_t PieceStorage::PiecesCompleteCount() const {
    return PiecesSavedToDiscCount();
}

void PieceStorage::PrintDownloadStatus() const {
    std::cout << "=== DOWNLOAD STATUS ===" << std::endl;
    std::cout << "Total pieces: " << total_piece_count << std::endl;
    std::cout << "Saved to disk: " << PiecesSavedToDiscCount() << std::endl;
    std::cout << "In queue: " << remaining_pieces_queue.size() << std::endl;
    std::cout << "Download complete: " << (IsDownloadComplete() ? "YES" : "NO") << std::endl;
}

void PieceStorage::PrintDetailedStatus() const {

    std::cout << "=== DETAILED STATUS ===" << std::endl;
    std::cout << "Total pieces: " << total_piece_count << std::endl;
    std::cout << "Saved: " << PiecesSavedToDiscCount() << std::endl;
    std::cout << "In queue: " << remaining_pieces_queue.size() << std::endl;
}


void PieceStorage::PrintMissingPieces() const {
    auto missing = GetMissingPieces();

    std::cout << "=== MISSING PIECES DIAGNOSTICS ===" << std::endl;
    std::cout << "Total pieces: " << total_piece_count << std::endl;
    std::cout << "Saved to disk: " << indices_of_pieces_saved_to_disk.size() << std::endl;
    std::cout << "In queue: " << remaining_pieces_queue.size() << std::endl;
    std::cout << "Missing pieces count: " << missing.size() << std::endl;

    if (!missing.empty()) {
        std::cout << "Missing pieces: ";
        for (size_t i = 0; i < std::min(missing.size(), size_t(20)); ++i) {
            std::cout << missing[i] << " ";
        }
        if (missing.size() > 20) {
            std::cout << "... (and " << (missing.size() - 20) << " more)";
        }
        std::cout << std::endl;
    }
}

bool PieceStorage::IsDownloadComplete() const {
    std::lock_guard<std::mutex> queueLock(queue_mutex);
    std::lock_guard<std::mutex> fileLock(file_mutex);

    return (indices_of_pieces_saved_to_disk.size() == total_piece_count);
}

void PieceStorage::ForceRequeueAllMissingPieces() {
    std::lock_guard<std::mutex> lock(queue_mutex);

    while (!remaining_pieces_queue.empty()) {
        remaining_pieces_queue.pop();
    }

    auto missing = GetMissingPieces();

    size_t requeued_count = 0;
    for (size_t piece_index : missing) {
        size_t pieceLength;
        if (piece_index == total_piece_count - 1) {
            pieceLength = torrent_file.length % torrent_file.piece_length;
            if (pieceLength == 0) {
                pieceLength = torrent_file.piece_length;
            }
        } else {
            pieceLength = torrent_file.piece_length;
        }

        auto piece = std::make_shared<Piece>(piece_index, pieceLength, torrent_file.piece_hashes[piece_index]);
        remaining_pieces_queue.push(piece);
        requeued_count++;

        std::cout << "FORCE REQUEUE: Added piece " << piece_index << " to queue" << std::endl;
    }

    std::cout << "FORCE REQUEUE: Added " << requeued_count << " pieces to queue" << std::endl;
    std::cout << "Queue size after requeue: " << remaining_pieces_queue.size() << std::endl;
}

std::vector<size_t> PieceStorage::GetMissingPieces() const {
    std::lock_guard<std::mutex> queueLock(queue_mutex);
    std::lock_guard<std::mutex> fileLock(file_mutex);

    std::vector<bool> pieceStatus(total_piece_count, false);

    for (size_t savedIndex : indices_of_pieces_saved_to_disk) {
        if (savedIndex < total_piece_count) {
            pieceStatus[savedIndex] = true;
        }
    }

    std::vector<size_t> missingPieces;
    for (size_t i = 0; i < total_piece_count; ++i) {
        if (!pieceStatus[i]) {
            missingPieces.push_back(i);
        }
    }

    return missingPieces;
}

void PieceStorage::ForceRequeueMissingPieces() {
    ForceRequeueAllMissingPieces();
}

size_t PieceStorage::PiecesSavedToDiscCount() const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return indices_of_pieces_saved_to_disk.size();
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
    if (!piece) return;

    std::lock_guard<std::mutex> lock(file_mutex);

    try {
        size_t file_offset = piece->GetIndex() * default_piece_length;
        std::string piece_data = piece->GetData();

        if (piece_data.size() != piece->GetLength()) {
            std::cerr << "ERROR: Piece " << piece->GetIndex()
                      << " data size mismatch: " << piece_data.size()
                      << " != " << piece->GetLength() << std::endl;
            return;
        }

        file.seekp(file_offset);
        file.write(piece_data.data(), piece_data.size());
        file.flush();

        if (std::find(indices_of_pieces_saved_to_disk.begin(),
                     indices_of_pieces_saved_to_disk.end(),
                     piece->GetIndex()) == indices_of_pieces_saved_to_disk.end()) {
            indices_of_pieces_saved_to_disk.push_back(piece->GetIndex());
            std::cout << "Saved piece " << piece->GetIndex() << " to disk ("
                      << piece_data.size() << " bytes)" << std::endl;
        } else {
            std::cout << "WARNING: Piece " << piece->GetIndex() << " already saved!" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Failed to save piece " << piece->GetIndex() << " to disk: "
                  << e.what() << std::endl;
        throw;
    }
}

size_t PieceStorage::TotalPiecesCount() const {
    return total_piece_count;
}

void PieceStorage::CloseOutputFile() {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (file.is_open()) {
        file.flush();
        file.close();
        std::cout << "Output file closed" << std::endl;
    }
}

size_t PieceStorage::getToDownloadCnt() const {
    return number_of_pieces_to_download;
}
