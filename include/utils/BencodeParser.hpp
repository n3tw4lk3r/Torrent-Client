#pragma once

#include <string>
#include <vector>

namespace utils {
class BencodeParser {
public:
    BencodeParser();
    ~BencodeParser() = default;

    std::vector<std::string> ParseFromFile(const std::string& filename);
    std::vector<std::string> ParseFromString(std::string str);
    std::string GetInfoHash();
    std::vector<std::string> GetPieceHashes();

private:
    std::string ReadFixedAmount(int amount);
    std::string ReadUntilDelimiter(char delimiter);
    std::string Process();
    void ProcessDict();
    void ProcessList();

    std::string to_decode;
    std::string info_hash;
    std::vector<std::string> parsed;
    std::vector<std::string> pieces_hashes;
    size_t index;
};
} // namespace utils
