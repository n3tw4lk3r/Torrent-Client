#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace utils {
int BytesToInt32(std::string_view bytes);
std::string Int32ToBytes(int value);
std::string CalculateSha1(std::string_view msg);
std::string HexEncode(std::string_view input);
std::string Int64ToBytes(uint64_t value);
uint64_t BytesToInt64(std::string_view bytes);
std::string BytesToHex(std::string_view bytes);
} // namespace utils
