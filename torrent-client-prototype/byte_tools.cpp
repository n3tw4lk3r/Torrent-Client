#include "byte_tools.h"

int BytesToInt(std::string_view bytes) {
    return int((unsigned char) (bytes[0]) << 24 |
               (unsigned char) (bytes[1]) << 16 |
               (unsigned char) (bytes[2]) << 8  |
               (unsigned char) (bytes[3]));
}

std::string IntToBytes(int Int) {
    std::string res;
    res += (unsigned char) (Int >> 24) & 0xFF;
    res += (unsigned char) (Int >> 16) & 0xFF;
    res += (unsigned char) (Int >> 8) & 0xFF;
    res += (unsigned char) Int & 0xFF;
    return res;
}

std::string CalculateSHA1(const std::string& msg) {
    char buffer[msg.size()];
    for (int i = 0; i < msg.size(); ++i) {
        buffer[i] = msg[i];
    }

    unsigned char sha[20];
    SHA1((unsigned char *) buffer, msg.size(), sha);

    std::string hashed;
    for (int i = 0; i < 20; ++i) {
        hashed += sha[i];
    }

    return hashed;
}

/* https://codereview.stackexchange.com/questions/78535/converting-array-of-bytes-to-the-hex-string-representation */
std::string HexEncode(const std::string& input) {
    std::stringstream ss;
    ss << std::hex;
    for (const char& ch : input) {
        ss << std::setw(2) << std::setfill('0') << (int) ch;
    }
    return ss.str();
}