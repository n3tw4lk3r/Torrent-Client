#include <string>

struct Block {
    static constexpr size_t kSize = 1 << 14; // 16KB
    
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
