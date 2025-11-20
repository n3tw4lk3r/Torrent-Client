#include "core/PieceStorage.hpp"
#include "core/Piece.hpp"
#include <iostream>
#include <cmath>
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

    file.seekp(torrent_file.length - 1);
    file.write("\0", 1);
    file.flush();
    std::cout << "Created output file: " << filename << " (" << torrent_file.length << " bytes)" << std::endl;
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::lock_guard<std::mutex> lock(queue_mutex);

    if (remaining_pieces_queue.empty()) {
        return nullptr;
    }

    PiecePtr piece = remaining_pieces_queue.front();
    remaining_pieces_queue.pop();
    
    std::cout << "DEBUG: Getting piece " << piece->GetIndex() 
              << " from queue. Remaining in queue: " << remaining_pieces_queue.size() << std::endl;
    
    return piece;
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    std::cout << "DEBUG: Processing piece " << piece->GetIndex() 
              << ", data size: " << piece->GetData().size() 
              << ", expected: " << piece->GetData().size() 
              << ", hash matches: " << piece->HashMatches() << std::endl;

    if (!piece->HashMatches()) {
        std::cout << "Hash mismatch for piece " << piece->GetIndex() << ", requeuing..." << std::endl;
        piece->Reset();
        Enqueue(piece);
        return;
    }

    SavePieceToDisk(piece);
}

void PieceStorage::Enqueue(const PiecePtr& piece) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    std::cout << "DEBUG: Requeuing piece " << piece->GetIndex() << std::endl;
    remaining_pieces_queue.push(piece);
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
    
    std::cout << "First 10 saved indices: ";
    for (size_t i = 0; i < std::min(indices_of_pieces_saved_to_disk.size(), size_t(10)); ++i) {
        std::cout << indices_of_pieces_saved_to_disk[i] << " ";
    }
    std::cout << std::endl;
}

bool PieceStorage::IsDownloadComplete() const {
    std::lock_guard<std::mutex> queueLock(queue_mutex);
    std::lock_guard<std::mutex> fileLock(file_mutex);
    
    bool complete = (indices_of_pieces_saved_to_disk.size() == total_piece_count);
    
    if (!complete) {
        std::cout << "DEBUG: Download incomplete - saved: " << indices_of_pieces_saved_to_disk.size() 
                  << ", total: " << total_piece_count 
                  << ", in queue: " << remaining_pieces_queue.size() << std::endl;
    }
    
    return complete;
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
    std::lock_guard<std::mutex> lock(queue_mutex);
    
    auto missing = GetMissingPieces();
    if (missing.empty()) {
        std::cout << "No missing pieces to requeue" << std::endl;
        return;
    }
    
    std::cout << "FORCE REQUEUE: Adding " << missing.size() << " missing pieces to queue" << std::endl;
    
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
        
        if (piece_index < torrent_file.piece_hashes.size()) {
            const std::string& hash = torrent_file.piece_hashes[piece_index];
            auto piece = std::make_shared<Piece>(piece_index, pieceLength, hash);
            remaining_pieces_queue.push(piece);
            std::cout << "FORCE REQUEUE: Added piece " << piece_index << " to queue" << std::endl;
        } else {
            std::cerr << "ERROR: Invalid piece index " << piece_index << " for requeue" << std::endl;
        }
    }
    
    std::cout << "Queue size after requeue: " << remaining_pieces_queue.size() << std::endl;
}

bool PieceStorage::QueueIsEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    bool empty = remaining_pieces_queue.empty();
    
    if (empty) {
        std::cout << "DEBUG: Queue is empty. Saved pieces: " 
                  << indices_of_pieces_saved_to_disk.size() << "/" << total_piece_count << std::endl;
    }
    
    return empty;
}

size_t PieceStorage::PiecesSavedToDiscCount() const {
    std::lock_guard<std::mutex> lock(file_mutex);
    size_t count = indices_of_pieces_saved_to_disk.size();
    std::cout << "DEBUG: Pieces saved count: " << count << "/" << total_piece_count << std::endl;
    return count;
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
    std::lock_guard<std::mutex> lock(file_mutex);

    try {
        size_t file_offset = piece->GetIndex() * default_piece_length;
        const std::string& piece_data = piece->GetData();

        if (piece_data.size() != piece->GetData().size()) {
            std::cerr << "ERROR: Piece " << piece->GetIndex() 
                      << " data size mismatch: " << piece_data.size() 
                      << " != " << piece->GetData().size() << std::endl;
        }

        file.seekp(file_offset);
        file.write(piece_data.data(), piece_data.size());
        file.flush();

        if (std::find(indices_of_pieces_saved_to_disk.begin(), 
                     indices_of_pieces_saved_to_disk.end(), 
                     piece->GetIndex()) != indices_of_pieces_saved_to_disk.end()) {
            std::cout << "WARNING: Piece " << piece->GetIndex() << " already saved!" << std::endl;
        } else {
            indices_of_pieces_saved_to_disk.push_back(piece->GetIndex());
        }

        std::cout << "Saved piece " << piece->GetIndex() << " ("
                  << piece_data.size() << " bytes) at offset " << file_offset 
                  << ". Total saved: " << indices_of_pieces_saved_to_disk.size() 
                  << "/" << total_piece_count << std::endl;

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
        file.close();
    }
}

size_t PieceStorage::getToDownloadCnt() const {
    return number_of_pieces_to_download;
}
