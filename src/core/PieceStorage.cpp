#include "core/PieceStorage.hpp"
#include "core/Piece.hpp"
#include <iostream>
#include <algorithm>

PieceStorage::PieceStorage(const TorrentFile& torrent_file, const std::filesystem::path& output_directory)
    : output_directory(output_directory)
    , default_piece_length(torrent_file.piece_length)
    , torrent_file(torrent_file) {

    total_piece_count = torrent_file.piece_hashes.size();


    for (size_t i = 0; i < total_piece_count; ++i) {
        size_t pieceLength = (i == total_piece_count - 1)
            ? (torrent_file.length % torrent_file.piece_length ?: torrent_file.piece_length)
            : torrent_file.piece_length;

        auto piece = std::make_shared<Piece>(i, pieceLength, torrent_file.piece_hashes[i]);
        remaining_pieces_queue.push(piece);
    }

    InitializeOutputFile();
}

void PieceStorage::InitializeOutputFile() {
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

}

size_t PieceStorage::GetMissingPiecesCount() const {
    return GetMissingPieces().size();
}

bool PieceStorage::HasActiveWork() const {
    return !QueueIsEmpty();
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::lock_guard<std::mutex> lock(queue_mutex);

    if (remaining_pieces_queue.empty()) {
        return nullptr;
    }

    PiecePtr piece = remaining_pieces_queue.front();
    remaining_pieces_queue.pop();
    return piece;
}

bool PieceStorage::IsPieceAlreadySaved(size_t piece_index) const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return std::find(indices_of_pieces_saved_to_disk.begin(),
                    indices_of_pieces_saved_to_disk.end(),
                    piece_index) != indices_of_pieces_saved_to_disk.end();
}

void PieceStorage::Enqueue(const PiecePtr& piece) {
    if (!piece) return;

    if (IsPieceAlreadySaved(piece->GetIndex())) {
        return;
    }

    std::lock_guard<std::mutex> lock(queue_mutex);
    piece->Reset();
    remaining_pieces_queue.push(piece);
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    if (!piece) return;

    if (!piece->HashMatches()) {
        Enqueue(piece);
        return;
    }

    SavePieceToDisk(piece);
}

bool PieceStorage::QueueIsEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return remaining_pieces_queue.empty();
}

void PieceStorage::PrintDownloadStatus() const {

}

void PieceStorage::PrintDetailedStatus() const {
}

void PieceStorage::PrintMissingPieces() const {

}

bool PieceStorage::IsDownloadComplete() const {
    std::lock_guard<std::mutex> fileLock(file_mutex);
    return indices_of_pieces_saved_to_disk.size() == total_piece_count;
}

void PieceStorage::ForceRequeueMissingPieces() {
    std::lock_guard<std::mutex> lock(queue_mutex);

    while (!remaining_pieces_queue.empty()) {
        remaining_pieces_queue.pop();
    }

    auto missing = GetMissingPieces();
    for (size_t piece_index : missing) {
        size_t pieceLength = (piece_index == total_piece_count - 1)
            ? (torrent_file.length % torrent_file.piece_length ?: torrent_file.piece_length)
            : torrent_file.piece_length;

        auto piece = std::make_shared<Piece>(piece_index, pieceLength, torrent_file.piece_hashes[piece_index]);
        remaining_pieces_queue.push(piece);
    }

}

std::vector<size_t> PieceStorage::GetMissingPieces() const {
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

size_t PieceStorage::PiecesSavedToDiscCount() const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return indices_of_pieces_saved_to_disk.size();
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
    if (!piece) return;

    std::lock_guard<std::mutex> lock(file_mutex);

    if (std::find(indices_of_pieces_saved_to_disk.begin(),
                 indices_of_pieces_saved_to_disk.end(),
                 piece->GetIndex()) != indices_of_pieces_saved_to_disk.end()) {
        return;
    }

    try {
        size_t file_offset = piece->GetIndex() * default_piece_length;
        std::string piece_data = piece->GetData();

        if (piece_data.size() != piece->GetLength()) {
            return;
        }

        file.seekp(file_offset);
        file.write(piece_data.data(), piece_data.size());
        file.flush();

        indices_of_pieces_saved_to_disk.push_back(piece->GetIndex());

    } catch (const std::exception& e) {
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
    }
}
