#include "utils/byte_tools.hpp"

#include <stdexcept>

#include <openssl/sha.h>

std::int32_t utils::BytesToInt32(std::string_view bytes) {
    if (bytes.size() < 4) {
        throw std::runtime_error("BytesToInt32: not enough bytes");
    }

    return (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[0])) << 24) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[1])) << 16) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[2])) << 8) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[3])));
}

std::string utils::Int32ToBytes(std::int32_t value) {
    std::string result(4, '\0');
    result[0] = static_cast<char>((value >> 24) & 0xFF);
    result[1] = static_cast<char>((value >> 16) & 0xFF);
    result[2] = static_cast<char>((value >> 8)  & 0xFF);
    result[3] = static_cast<char>( value        & 0xFF);

    return result;
}

std::string utils::CalculateSha1(std::string_view msg) {
    unsigned char hash[SHA_DIGEST_LENGTH];

    SHA1(reinterpret_cast<const unsigned char*>(msg.data()),
         msg.size(),
         hash);

    return std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);
}

std::string utils::HexEncode(std::string_view input) {
    static constexpr char hex_table[] = "0123456789abcdef";

    std::string result;
    result.resize(input.size() * 2);

    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        result[(i << 1)] = hex_table[c >> 4];
        result[(i << 1) | 1] = hex_table[c & 0x0F];
    }

    return result;
}

std::string utils::Int64ToBytes(std::uint64_t value) {
    std::string result(8, '\0');
    for (int i = 7; i >= 0; --i) {
        result[i] = static_cast<unsigned char>(value & 0xFF);
        value >>= 8;
    }
    return result;
}

std::uint64_t utils::BytesToInt64(std::string_view bytes) {
    if (bytes.size() < 8) {
        throw std::runtime_error("BytesToInt64: not enough bytes");
    }

    uint64_t result = 0;

    for (int i = 0; i < 8; ++i) {
        result = (result << 8) | static_cast<unsigned char>(bytes[i]);
    }

    return result;
}

std::string utils::BytesToHex(std::string_view bytes) {
    return HexEncode(bytes);
}
