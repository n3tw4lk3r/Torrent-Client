#pragma once

#include <string>
#include <string_view>
#include <cstdint>

namespace utils {
    int BytesToInt(std::string_view bytes);
    std::string IntToBytes(int value);
    std::string CalculateSHA1(const std::string& msg);
    std::string HexEncode(const std::string& input);
    std::string Int64ToBytes(uint64_t value);
    uint64_t BytesToInt64(const std::string& bytes);
    std::string BytesToHex(const std::string& bytes);
}
