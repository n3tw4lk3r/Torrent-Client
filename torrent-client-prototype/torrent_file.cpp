#include "torrent_file.h"
#include "bencode.h"
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <variant>
#include <sstream>

TorrentFile LoadTorrentFile(const std::string& filename) {
    std::cout << "Loading torrent file...\n";
    TorrentFile result;
    
    BencodeParser myParser;
    auto res = myParser.parseFromFile(filename);

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
            result.pieceLength = stol(res[i]);
            continue;
        }

        if (res[i] == "length") {
            ++i;
            // std::cout << res[i] << '\n';
            result.length = stol(res[i]);
            continue;
        }

        if (res[i] == "name") {
            ++i;
            result.name = res[i];
            continue;
        }
    }

    result.infoHash = myParser.getHash();
    result.pieceHashes = myParser.getPieceHashes();
    return result;
}
