#include "utils/byte_tools.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <openssl/sha.h>
#include <iostream>

int utils::BytesToInt(std::string_view bytes) {
    if (bytes.size() < 4)
        throw std::runtime_error("BytesToInt: not enough bytes");

    return (static_cast<unsigned char>(bytes[0]) << 24) |
           (static_cast<unsigned char>(bytes[1]) << 16) |
           (static_cast<unsigned char>(bytes[2]) << 8)  |
           (static_cast<unsigned char>(bytes[3]));
}

std::string utils::IntToBytes(int value) {
    std::string result(4, '\0');
    result[0] = static_cast<unsigned char>((value >> 24) & 0xFF);
    result[1] = static_cast<unsigned char>((value >> 16) & 0xFF);
    result[2] = static_cast<unsigned char>((value >> 8)  & 0xFF);
    result[3] = static_cast<unsigned char>( value        & 0xFF);
    return result;
}

std::string utils::CalculateSHA1(const std::string& msg) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(msg.data()), msg.size(), hash);

    return std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);
}

std::string utils::HexEncode(const std::string& input) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (unsigned char c : input) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}

std::string utils::Int64ToBytes(uint64_t value) {
    std::string result(8, '\0');
    for (int i = 7; i >= 0; --i) {
        result[i] = static_cast<unsigned char>(value & 0xFF);
        value >>= 8;
    }
    return result;
}

uint64_t utils::BytesToInt64(const std::string& bytes) {
    if (bytes.size() < 8)
        throw std::runtime_error("BytesToInt64: not enough bytes");

    uint64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result = (result << 8) | static_cast<unsigned char>(bytes[i]);
    }
    return result;
}

std::string utils::BytesToHex(const std::string& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (unsigned char c : bytes) {
        ss << std::setw(2) << static_cast<int>(c);
    }
    return ss.str();
}
