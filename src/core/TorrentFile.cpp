#include "core/TorrentFile.hpp"

#include <vector>

#include <openssl/sha.h>

#include "utils/BencodeParser.hpp"

TorrentFile LoadTorrentFile(const std::string& filename) {
    TorrentFile result;

    utils::BencodeParser bencode_parser;
    auto res = bencode_parser.ParseFromFile(filename);

    for (size_t i = 0; i < res.size(); ++i) {
        if (res[i] == "announce") {
            ++i;
            result.announce = res[i];
            continue;
        }

        if (res[i] == "comment") {
            ++i;
            result.comment = res[i];
            continue;
        }

        if (res[i] == "piece length") {
            ++i;
            result.piece_length = std::stol(res[i]);
            continue;
        }

        if (res[i] == "length") {
            ++i;
            result.length = std::stol(res[i]);
            continue;
        }

        if (res[i] == "name") {
            ++i;
            result.name = res[i];
            continue;
        }
    }

    result.info_hash = bencode_parser.GetInfoHash();
    result.piece_hashes = bencode_parser.GetPieceHashes();
    return result;
}
