#include "core/Piece.hpp"
#include "utils/byte_tools.hpp"
#include <iostream>
#include <algorithm>

Piece::Piece(size_t index, size_t length, const std::string& hash)
    : index(index), length(length), hash(hash), bytes_downloaded(0) {

    size_t offset = 0;
    while (offset < length) {
        size_t block_length = std::min(kBlockSize, length - offset);
        blocks.push_back(Block{index, offset, block_length, Block::kMissing, ""});
        offset += block_length;
    }
}

bool Piece::HashMatches() const {
    if (!AllBlocksRetrieved()) {
        return false;
    }

    std::string piece_data = GetData();
    std::string calculated_hash = utils::CalculateSHA1(piece_data);
    bool matches = (calculated_hash == hash);

    return matches;
}

Block* Piece::GetFirstMissingBlock() {
    for (auto& block : blocks) {
        if (block.status == Block::kMissing) {
            block.status = Block::kPending;
            return &block;
        }
    }
    return nullptr;
}

size_t Piece::GetIndex() const {
    return index;
}

void Piece::SaveBlock(size_t blockOffset, std::string block_data) {
    for (auto& block : blocks) {
        if (block.offset == blockOffset) {
            if (block.status != Block::kPending) {
                throw std::runtime_error("Block at offset " + std::to_string(blockOffset) +
                                       " is not in pending state");
            }

            block.data = std::move(block_data);
            block.status = Block::kRetrieved;
            bytes_downloaded += block.data.size();
            return;
        }
    }
    throw std::runtime_error("Block not found at offset " + std::to_string(blockOffset));
}

bool Piece::AllBlocksRetrieved() const {
    return std::all_of(blocks.begin(), blocks.end(), [](const Block& block) {
        return block.status == Block::kRetrieved;
    });
}

std::string Piece::GetData() const {
    std::string result;
    result.reserve(length);

    for (const auto& block : blocks) {
        if (block.status == Block::kRetrieved) {
            result += block.data;
        } else {
            result.append(block.length, '\0');
        }
    }

    return result;
}

std::string Piece::GetDataHash() const {
    return utils::CalculateSHA1(GetData());
}

const std::string& Piece::GetHash() const {
    return hash;
}

void Piece::Reset() {
    bytes_downloaded = 0;
    for (auto& block : blocks) {
        block.status = Block::kMissing;
        block.data.clear();
    }
}

bool Piece::IsDownloading() const {
    return std::any_of(blocks.begin(), blocks.end(), [](const Block& block) {
        return block.status == Block::kPending;
    });
}

bool Piece::IsComplete() const {
    return AllBlocksRetrieved();
}

size_t Piece::GetLength() const {
    return length;
}

size_t Piece::GetBytesDownloaded() const {
    return bytes_downloaded;
}
