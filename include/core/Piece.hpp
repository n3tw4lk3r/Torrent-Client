#pragma once

#include <string>
#include <vector>
#include <memory>

constexpr size_t kBlockSize = 1 << 14; // 16KB

struct Block {
    enum Status {
        kMissing = 0,
        kPending,
        kRetrieved,
    };

    size_t piece;
    size_t offset;
    size_t length;
    Status status;
    std::string data;
};

class Piece {
public:
    Piece(size_t index, size_t length, const std::string& hash);

    bool HashMatches() const;
    Block* GetFirstMissingBlock();
    size_t GetIndex() const;
    void SaveBlock(size_t blockOffset, std::string data);
    bool AllBlocksRetrieved() const;
    std::string GetData() const;
    std::string GetDataHash() const;
    const std::string& GetHash() const;
    void Reset();

    bool IsDownloading() const;
    bool IsComplete() const;
    size_t GetLength() const;
    size_t GetBytesDownloaded() const;

private:
    size_t index;
    size_t length;
    std::string hash;
    std::vector<Block> blocks;
    size_t bytes_downloaded;
};

using PiecePtr = std::shared_ptr<Piece>;
