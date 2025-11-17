#pragma once

#include "peer.h"
#include "torrent_file.h"
#include "byte_tools.h"
#include <string>
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iostream>

class BencodeParser {
    std::string toDecode_, infoHash_;
    std::vector<std::string> parsed_, piecesHashes_;
    int idx_;

    std::string readFixedAmount(int amount);
    std::string readUntilDelimiter(char delimiter);
    std::string process();
    void processDict();
    void processList();

public:
    BencodeParser();
    ~BencodeParser() = default;

    std::vector<std::string> parseFromFile(const std::string& filename);
    std::vector<std::string> parseFromString(std::string str);
    std::string getHash();
    std::vector<std::string> getPieceHashes();
};
