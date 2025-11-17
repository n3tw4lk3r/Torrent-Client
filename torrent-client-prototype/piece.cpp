#include "byte_tools.h"
#include "piece.h"
#include <iostream>
#include <algorithm>

Piece::Piece(size_t index, size_t length, std::string hash)
    : index_(index)
    , length_(length)
    , hash_(hash)
    {
        size_t bytesUsed = 0, offset = 0;
        for (; bytesUsed < length - BLOCK_SIZE; bytesUsed += BLOCK_SIZE) {
            blocks_.push_back({ index, offset, BLOCK_SIZE, Block::Status(0), "" });
            offset += BLOCK_SIZE;
        }
        blocks_.push_back({ index, offset, length_ - bytesUsed, Block::Status(0), "" });
    }

bool Piece::HashMatches() const {
    return GetDataHash() == hash_;
}

Block* Piece::FirstMissingBlock() {
    for (Block& block : blocks_) {
        if (block.status == Block::Status::Missing) {
            block.status = Block::Status::Pending;
            return &block; // perhaps shouldn't use raw ptr
        }
    }
    return nullptr;
}

size_t Piece::GetIndex() const {
    return index_;
}

void Piece::SaveBlock(size_t blockOffset, std::string data) {
    // std::cout << "Saving block " << blockOffset << '\n';
    for (Block& block : blocks_) {
        if (block.offset == blockOffset) {
            block.data = data;
            block.status = Block::Status::Retrieved;
            return;
        }
    }
}

bool Piece::AllBlocksRetrieved() const {
    for (const Block& block : blocks_) {
        if (block.status != Block::Status::Retrieved) {
            return false;
        }
    }
    return true;
}

std::string Piece::GetData() const {
    std::string data;
    for (const Block& block : blocks_) {
        data += block.data;
    }
    return data;
}

std::string Piece::GetDataHash() const {
    return CalculateSHA1(GetData());
}

const std::string& Piece::GetHash() const {
    return hash_;
}

void Piece::Reset() {
    // std::cout << "piece reset called\n";
    for (Block& block : blocks_) {
        block.data.clear();
        block.status == Block::Status::Missing;
    }
}
