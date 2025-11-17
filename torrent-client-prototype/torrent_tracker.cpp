#include "torrent_tracker.h"
#include "bencode.h"
#include "byte_tools.h"
#include <cpr/cpr.h>

TorrentTracker::TorrentTracker(const std::string& url) : url_(url) {}

void TorrentTracker::UpdatePeers(const TorrentFile& tf, std::string peerId, int port) {
    std::cout << "url: " << url_ << std::endl;
    cpr::Response trackerResponse = cpr::Get(
        cpr::Url{url_},
        cpr::Parameters {
                {"info_hash", tf.infoHash},
                {"peer_id", peerId},
                {"port", std::to_string(port)},
                {"uploaded", std::to_string(0)},
                {"downloaded", std::to_string(0)},
                {"left", std::to_string(tf.length)},
                {"compact", std::to_string(1)}
        },
        cpr::Timeout{20000}
    );

    if (trackerResponse.status_code != 200) {
        std::cout << "update peers error: " << trackerResponse.status_code << '\n';
        return;
    }

    std::string responseString = trackerResponse.text;
    BencodeParser myParser;
    std::vector<std::string> parsed = myParser.parseFromString(responseString);
    std::string ipString;
    for (int i = 0; i < parsed.size(); ++i) {
        if (parsed[i] == "peers") {
            ipString = parsed[i + 1];
            break;
        }
    }

    for (int i = 0; i < ipString.size(); i += 6) {
        std::string ip;
        int j = 0;
        while (j < 3) {
            ip += std::to_string(uint8_t( (unsigned char) (ipString[i + j]))) + '.';
            ++j;
        }

        ip += std::to_string(uint8_t( (unsigned char) (ipString[i + j])));
        int port = (uint16_t( (unsigned char) (ipString[i + 4])) << 8) + uint16_t( (unsigned char) (ipString[i + 5]));
        peers_.push_back({ip, port});
    }
}

/*
 * Отдает полученный ранее список пиров
 */
const std::vector<Peer>& TorrentTracker::GetPeers() const {
    return peers_;
}