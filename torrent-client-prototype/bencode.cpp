#include "bencode.h"

std::string BencodeParser::readFixedAmount(int amount) {
    std::string res = toDecode_.substr(idx_, amount);
    idx_ += amount;
    return res;
}

std::string BencodeParser::readUntilDelimiter(char delimiter) {
    std::string res;
    while (toDecode_[idx_] != delimiter) {
        res += toDecode_[idx_];
        ++idx_;
    }

    ++idx_;
    return res;
}

std::string BencodeParser::process() {
    char ch = toDecode_[idx_];
    if ('0' <= ch && ch <= '9') {
        int str_len = stoi(readUntilDelimiter(':'));
        std::string tmp = readFixedAmount(str_len);
        parsed_.push_back(tmp);
        return tmp;
    }

    if (ch == 'i') {
        ++idx_;
        parsed_.push_back(readUntilDelimiter('e'));
        return "";
    }

    if (ch == 'd') {
        processDict();
        return "";
    }

    if (ch == 'l') {
        processList();
        return "";
    }

    return "";
}

void BencodeParser::processDict() {
    ++idx_;
    std::string key_name;
    int stInd = -1, endInd = -1;
    bool flag = false;

    while (toDecode_[idx_] != 'e') {
        if (key_name.empty()) {
            key_name = process();

            if (key_name == "info") {
                stInd = idx_;
                flag = true;
            }
        }

        else {
            process();
            if (flag) {
                endInd = idx_;
                flag = false;
            }

            key_name.clear();
        }
    }
    
    ++idx_;

    if (stInd != -1 && endInd != -1) {
        std::string toHash;
        for (int i = stInd; i < endInd; ++i) {
            toHash += toDecode_[i];
        }
        infoHash_ = CalculateSHA1(toHash);
    }
}

void BencodeParser::processList() {
    ++idx_;
    while (toDecode_[idx_] != 'e') {
        process();
    }
    ++idx_;
}

BencodeParser::BencodeParser() : idx_(0) {}

std::vector<std::string> BencodeParser::parseFromFile(const std::string& filename) {
    std::cout << "bencode: " << filename << std::endl;
    char buffer;
    std::fstream inputfstream(filename, std::fstream::in);

    while (inputfstream >> std::noskipws >> buffer) {
        toDecode_ += buffer;
    }

    process();
    return parsed_;
}

std::vector<std::string> BencodeParser::parseFromString(std::string str) { 
    toDecode_ = str;
    process();
    return parsed_;
}

std::string BencodeParser::getHash() {
    return infoHash_;
}

std::vector<std::string> BencodeParser::getPieceHashes() {
    std::cout << "getpiecehashes called" << std::endl;
    int piecesStart = -1, piecesEnd;
    for (int i = 0; i < toDecode_.size(); ++i) {
        if (toDecode_.substr(i, 8) == "6:pieces") {
            std::cout << "start found" << std::endl;
            idx_ = i + 7;
            readUntilDelimiter(':');
            piecesStart = idx_;
        }
        if ((piecesStart != -1) && (toDecode_.substr(i, 11) == "e8:url-list" || toDecode_.substr(i, 2) == "ee")) {
            std::cout << "end found" << std::endl;
            piecesEnd = i - 1;
            break;
        }
    }
    std::cout << piecesStart << ' ' << piecesEnd << "!\n";
    int i = piecesStart;
    for (int cnt = 0; cnt < (piecesEnd - piecesStart + 1) / 20; ++cnt) {
        std::string pieceHash;
        for (int j = 0; j < 20; ++j, ++i) {
            pieceHash += toDecode_[i];
        }
        piecesHashes_.push_back(pieceHash);
    }

    std::cout << "total size: " << piecesHashes_.size() << std::endl;
    return piecesHashes_;
}
