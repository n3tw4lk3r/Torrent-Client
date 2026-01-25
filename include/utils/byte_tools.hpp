#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace utils {
int BytesToInt32(std::string_view bytes);
std::string Int32ToBytes(int value);
std::string CalculateSha1(const std::string& msg);
std::string HexEncode(const std::string& input);
std::string Int64ToBytes(uint64_t value);
uint64_t BytesToInt64(const std::string& bytes);
std::string BytesToHex(const std::string& bytes);
}
