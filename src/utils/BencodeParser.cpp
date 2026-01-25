#include "utils/BencodeParser.hpp"

#include <fstream>
#include <stdexcept>

#include "utils/byte_tools.hpp"

std::string utils::BencodeParser::ReadFixedAmount(int amount) {
    if (index + amount > to_decode.size()) {
        throw std::runtime_error("Not enough data to read fixed amount");
    }
    std::string result = to_decode.substr(index, amount);
    index += amount;
    return result;
}

std::string utils::BencodeParser::ReadUntilDelimiter(char delimiter) {
    std::string result;
    while (index < to_decode.size() && to_decode[index] != delimiter) {
        result += to_decode[index];
        ++index;
    }

    if (index >= to_decode.size()) {
        throw std::runtime_error("Delimiter not found");
    }

    ++index;
    return result;
}

std::string utils::BencodeParser::Process() {
    if (index >= to_decode.size()) {
        throw std::runtime_error("Unexpected end of input");
    }

    char current_char = to_decode[index];
    if ('0' <= current_char && current_char <= '9') {
        int string_length = std::stoi(ReadUntilDelimiter(':'));
        std::string temp = ReadFixedAmount(string_length);
        parsed.push_back(temp);
        return temp;
    }

    if (current_char == 'i') {
        ++index;
        parsed.push_back(ReadUntilDelimiter('e'));
        return "";
    }

    if (current_char == 'd') {
        ProcessDict();
        return "";
    }

    if (current_char == 'l') {
        ProcessList();
        return "";
    }

    throw std::runtime_error("Invalid bencode character: " + std::string(1, current_char));
}

void utils::BencodeParser::ProcessDict() {
    ++index;
    std::string key_name;
    int start_index = -1;
    int end_index = -1;
    bool found_info = false;
    while (index < to_decode.size() && to_decode[index] != 'e') {
        if (key_name.empty()) {
            key_name = Process();

            if (key_name == "info") {
                start_index = index;
                found_info = true;
            }
        } else {
            Process();
            if (found_info) {
                end_index = index;
                found_info = false;
            }

            key_name.clear();
        }
    }

    if (index >= to_decode.size()) {
        throw std::runtime_error("Unexpected end of dictionary");
    }

    ++index;

    if (start_index != -1 && end_index != -1) {
        std::string to_hash;
        for (int i = start_index; i < end_index; ++i) {
            to_hash += to_decode[i];
        }
        info_hash = CalculateSha1(to_hash);
    }
}

void utils::BencodeParser::ProcessList() {
    ++index;
    while (index < to_decode.size() && to_decode[index] != 'e') {
        Process();
    }
    
    if (index >= to_decode.size()) {
        throw std::runtime_error("Unexpected end of list");
    }
    
    ++index;
}

utils::BencodeParser::BencodeParser() : index(0) {}

std::vector<std::string> utils::BencodeParser::ParseFromFile(const std::string& filename) {
    char buffer;
    std::ifstream input_file(filename, std::ios::binary);

    if (!input_file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    while (input_file.read(&buffer, 1)) {
        to_decode += buffer;
    }

    Process();
    return parsed;
}

std::vector<std::string> utils::BencodeParser::ParseFromString(std::string str) {
    to_decode = std::move(str);
    Process();
    return parsed;
}

std::string utils::BencodeParser::GetInfoHash() {
    return info_hash;
}

std::vector<std::string> utils::BencodeParser::GetPieceHashes() {
    if (!pieces_hashes.empty()) {
        return pieces_hashes;
    }

    for (size_t i = 0; i < parsed.size(); ++i) {
        if (parsed[i] == "pieces" && i + 1 < parsed.size()) {
            std::string pieces_data = parsed[i + 1];

            constexpr size_t kHashSize = 20;
            for (size_t j = 0; j + kHashSize <= pieces_data.size(); j += kHashSize) {
                pieces_hashes.push_back(pieces_data.substr(j, kHashSize));
            }

            break;
        }
    }

    return pieces_hashes;
}
