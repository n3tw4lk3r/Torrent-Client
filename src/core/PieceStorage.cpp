#include "core/PieceStorage.hpp"

#include <algorithm>
#include <stdexcept>

PieceStorage::PieceStorage(const TorrentFile& torrent_file,
                           const std::filesystem::path& output_directory) :
      output_directory(output_directory),
      default_piece_length(torrent_file.piece_length),
      total_piece_count(torrent_file.piece_hashes.size()),
      torrent_file(torrent_file)
{
    for (size_t i = 0; i < total_piece_count; ++i) {
        size_t len;
        if (i + 1 == total_piece_count) {
            len = torrent_file.length - i * torrent_file.piece_length;
        } else {
            len = torrent_file.piece_length;
        }

        remaining_pieces_queue.push(
            std::make_shared<Piece>(i, len, torrent_file.piece_hashes[i])
        );
    }

    InitializeOutputFile();
}

void PieceStorage::InitializeOutputFile() {
    std::filesystem::create_directories(output_directory);
    auto filename = (output_directory / torrent_file.name).string();

    file.open(filename, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open output file");
    }

    file.seekp(torrent_file.length - 1);
    file.write("", 1);
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    if (remaining_pieces_queue.empty()) {
        return nullptr;
    }

    auto piece = remaining_pieces_queue.front();
    remaining_pieces_queue.pop();
    return piece;
}

void PieceStorage::Enqueue(const PiecePtr& piece) {
    if (!piece) {
        return;
    }

    if (IsPieceAlreadySaved(piece->GetIndex())) {
        return;
    }

    piece->Reset();
    std::lock_guard<std::mutex> lock(queue_mutex);
    remaining_pieces_queue.push(piece);
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    if (!piece || !piece->HashMatches()) {
        return Enqueue(piece);
    }

    SavePieceToDisk(piece);
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
    std::lock_guard<std::mutex> lock(file_mutex);

    if (saved_pieces.contains(piece->GetIndex())) {
        return;
    }

    file.seekp(piece->GetIndex() * default_piece_length);
    const auto& data = piece->GetData();
    file.write(data.data(), data.size());

    saved_pieces.insert(piece->GetIndex());
}

bool PieceStorage::QueueIsEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return remaining_pieces_queue.empty();
}

bool PieceStorage::IsPieceAlreadySaved(size_t index) const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return saved_pieces.contains(index);
}

bool PieceStorage::IsDownloadComplete() const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return saved_pieces.size() == total_piece_count;
}

bool PieceStorage::HasActiveWork() const {
    return !QueueIsEmpty();
}

size_t PieceStorage::TotalPiecesCount() const {
    return total_piece_count;
}

size_t PieceStorage::PiecesSavedToDiscCount() const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return saved_pieces.size();
}

std::vector<size_t> PieceStorage::GetMissingPieces() const {
    std::lock_guard<std::mutex> lock(file_mutex);
    std::vector<size_t> missing;
    missing.reserve(total_piece_count - saved_pieces.size());

    for (size_t i = 0; i < total_piece_count; ++i) {
        if (!saved_pieces.contains(i)) {
            missing.push_back(i);
        }
    }
    return missing;
}

void PieceStorage::ForceRequeueMissingPieces() {
    std::lock_guard<std::mutex> qlock(queue_mutex);
    std::lock_guard<std::mutex> flock(file_mutex);

    std::queue<PiecePtr> empty;
    std::swap(remaining_pieces_queue, empty);

    for (size_t i = 0; i < total_piece_count; ++i) {
        if (saved_pieces.contains(i)) {
            continue;
        }

        size_t len;
        if (i + 1 == total_piece_count) {
            len = torrent_file.length - i * torrent_file.piece_length;
        } else {
            len = torrent_file.piece_length;
        }

        remaining_pieces_queue.push(
            std::make_shared<Piece>(i, len, torrent_file.piece_hashes[i])
        );
    }
}

void PieceStorage::CloseOutputFile() {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (file.is_open()) {
        file.flush();
        file.close();
    }
}
