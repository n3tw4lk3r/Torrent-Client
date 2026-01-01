#include "core/TorrentFile.hpp"
#include "utils/BencodeParser.hpp"
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <variant>
#include <sstream>

TorrentFile LoadTorrentFile(const std::string& filename) {
    TorrentFile result;

    utils::BencodeParser myParser;
    auto res = myParser.ParseFromFile(filename);

    for (int i = 0; i < res.size(); ++i) {
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
            result.piece_length = stol(res[i]);
            continue;
        }

        if (res[i] == "length") {
            ++i;
            result.length = stol(res[i]);
            continue;
        }

        if (res[i] == "name") {
            ++i;
            result.name = res[i];
            continue;
        }
    }

    result.info_hash = myParser.GetHash();
    result.piece_hashes = myParser.GetPieceHashes();
    return result;
}
