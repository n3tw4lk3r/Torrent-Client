#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace utils {

std::int32_t BytesToInt32(std::string_view bytes);
std::string Int32ToBytes(std::int32_t value);
std::string Int64ToBytes(std::uint64_t value);
std::uint64_t BytesToInt64(std::string_view bytes);

std::string CalculateSha1(std::string_view msg);
std::string HexEncode(std::string_view input);
std::string BytesToHex(std::string_view bytes);

} // namespace utils
