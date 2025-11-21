#include "utils/BencodeParser.hpp"
#include "utils/byte_tools.hpp"
#include <iostream>

std::string utils::BencodeParser::ReadFixedAmount(int amount) {
    std::string result = to_decode.substr(index, amount);
    index += amount;
    return result;
}

std::string utils::BencodeParser::ReadUntilDelimiter(char delimiter) {
    std::string result;
    while (to_decode[index] != delimiter) {
        result += to_decode[index];
        ++index;
    }

    ++index;
    return result;
}

std::string utils::BencodeParser::Process() {
    char current_char = to_decode[index];
    if ('0' <= current_char && current_char <= '9') {
        int string_length = stoi(ReadUntilDelimiter(':'));
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

    return "";
}

void utils::BencodeParser::ProcessDict() {
    ++index;
    std::string key_name;
    int start_index = -1;
    int end_index = -1;
    bool flag = false;
    while (to_decode[index] != 'e') {
        if (key_name.empty()) {
            key_name = Process();

            if (key_name == "info") {
                start_index = index;
                flag = true;
            }
        }

        else {
            Process();
            if (flag) {
                end_index = index;
                flag = false;
            }

            key_name.clear();
        }
    }

    ++index;

    if (start_index != -1 && end_index != -1) {
        std::string to_hash;
        for (int i = start_index; i < end_index; ++i) {
            to_hash += to_decode[i];
        }
        info_hash = CalculateSHA1(to_hash);
    }
}

void utils::BencodeParser::ProcessList() {
    ++index;
    while (to_decode[index] != 'e') {
        Process();
    }
    ++index;
}

utils::BencodeParser::BencodeParser() : index(0) {}

std::vector<std::string> utils::BencodeParser::ParseFromFile(const std::string& filename) {
    std::cout << "bencode: " << filename << std::endl;
    char buffer;
    std::fstream inputfstream(filename, std::fstream::in);

    while (inputfstream >> std::noskipws >> buffer) {
        to_decode += buffer;
    }

    Process();
    return parsed;
}

std::vector<std::string> utils::BencodeParser::ParseFromString(std::string string) {
    to_decode = string;
    Process();
    return parsed;
}

std::string utils::BencodeParser::GetHash() {
    return info_hash;
}

std::vector<std::string> utils::BencodeParser::GetPieceHashes() {
    for (size_t i = 0; i < parsed.size(); ++i) {
        if (parsed[i] == "pieces" && i + 1 < parsed.size()) {
            std::string pieces_data = parsed[i + 1];

            for (size_t j = 0; j + 20 <= pieces_data.size(); j += 20) {
                pieces_hashes.push_back(pieces_data.substr(j, 20));
            }

            std::cout << "Extracted " << pieces_hashes.size() << " piece hashes" << std::endl;
            break;
        }
    }

    return pieces_hashes;
}
