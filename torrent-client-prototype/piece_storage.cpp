#include "piece_storage.h"
#include <iostream>

PieceStorage::PieceStorage(const TorrentFile& tf, const std::filesystem::path& outputDirectory, int piecesToDownloadCnt)
    : defaultPieceLength_(tf.pieceLength)
    , toDownloadCnt_(piecesToDownloadCnt)
    {   
        for (int i = 0; i < tf.pieceHashes.size(); ++i, ++totalPieceCount_) {
            size_t len;
            if (i == tf.pieceHashes.size() - 1) {
                len = tf.length % tf.pieceLength;
                if (len == 0) {
                    len = tf.pieceLength;
                }
            }
            else {
                len = tf.pieceLength;
            }
            PiecePtr ptr = std::make_shared<Piece>(Piece(i, len, tf.pieceHashes[i]));
            if (i < piecesToDownloadCnt)
                remainPieces_.push(ptr);
        }

        std::string filename = (outputDirectory / tf.name).generic_string();
        // std::cout << "filename: " << filename << '\n';
        file_.open(filename, std::ios::out | std::ios::binary);
        file_.seekp(tf.length - 1);
        file_.write("\0", 1);
    }

PiecePtr PieceStorage::GetNextPieceToDownload() {
    // std::cout << "getnextpiece called\n";
    std::lock_guard guard(queueMutex_);
    PiecePtr ptr = nullptr;
    if (!remainPieces_.empty()) {
        ptr = remainPieces_.front();
        remainPieces_.pop();
    }
    return ptr;
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    // std::cout << "pieceprocessed called\n";
    if (!piece->HashMatches()) {
        std::cout << "mismatching piece hash\n";
        piece->Reset();
        Enqueue(piece);
        return;
    }

    SavePieceToDisk(piece);
}

void PieceStorage::Enqueue(const PiecePtr& piece) {
    std::lock_guard guard(queueMutex_);
    std::cout << "enqueue called\n";
    remainPieces_.push(piece);
}

bool PieceStorage::QueueIsEmpty() const {
    // std::cout << "queueisempty called\n";
    std::lock_guard guard(queueMutex_);
    return remainPieces_.empty();
}

size_t PieceStorage::TotalPiecesCount() const {
    return totalPieceCount_;
}

size_t PieceStorage::PiecesSavedToDiscCount() const {
    // std::cout << "savedcnt called: ";
    std::lock_guard guard(fileMutex_); // wait until pending save finishes
    return piecesSavedToDiskIndices_.size();
}

void PieceStorage::CloseOutputFile() {
    std::cout << "close called\n";
    std::lock_guard guard(fileMutex_); // wait until pending save finishes
    file_.close();
}

const std::vector<size_t>& PieceStorage::GetPiecesSavedToDiscIndices() const {
    std::cout << "getindices called\n";
    std::lock_guard guard(fileMutex_); // wait until pending save finishes
    return piecesSavedToDiskIndices_;
}

size_t PieceStorage::PiecesInProgressCount() const {
    std::cout << "pieceinprogresscnt called: ";
    std::lock_guard guard(queueMutex_);
    std::lock_guard guard2(fileMutex_);
    std::cout << "total: " << totalPieceCount_ << " --- left: " << remainPieces_.size() << " --- in progress: " << totalPieceCount_ - remainPieces_.size() - piecesSavedToDiskIndices_.size() << " --- saved: " << piecesSavedToDiskIndices_.size() << std::endl;
    return totalPieceCount_ - remainPieces_.size() - piecesSavedToDiskIndices_.size();
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
    std::lock_guard guard(fileMutex_);
    file_.seekp(piece->GetIndex() * defaultPieceLength_);
    file_.write(piece->GetData().data(), piece->GetData().size());
    piecesSavedToDiskIndices_.push_back(piece->GetIndex());
    std::cout << "Saved piece " << piece->GetIndex() << ' ' << piece->GetData().size() << std::endl;
}

size_t PieceStorage::getToDownloadCnt() const {
    return toDownloadCnt_;
} 