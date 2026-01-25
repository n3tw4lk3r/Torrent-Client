#include "core/TorrentTracker.hpp"

#include <cpr/cpr.h>

#include "core/UdpTracker.hpp"
#include "utils/BencodeParser.hpp"

TorrentTracker::TorrentTracker(const std::string& url) : tracker_url(url) {}

void TorrentTracker::UpdatePeers(const TorrentFile& torrent_file,
                                 const std::string& peer_id,
                                 int port) {
    std::vector<std::string> all_trackers = { tracker_url };

    if (IsUdpTracker()) {
        for (const auto& backup : kBackupUdpTrackers) {
            if (backup != tracker_url) {
                all_trackers.push_back(std::string(backup));
            }
        }
    }

    for (size_t i = 0; i < all_trackers.size() && peers.empty(); ++i) {
        const auto& current_tracker = all_trackers[i];

        try {
            if (IsUdpTracker(current_tracker)) {
                UpdatePeersUdp(torrent_file, peer_id, port, current_tracker);
            } else {
                UpdatePeersHttp(torrent_file, peer_id, port, current_tracker);
            }

            if (!peers.empty()) {
                break;
            }

        } catch (const std::exception& e) {
            // ignore tracker errors, try next one
        }
    }

    if (peers.empty()) {
        throw std::runtime_error("All trackers failed - no peers found");
    }
}

bool TorrentTracker::IsUdpTracker() const {
    return IsUdpTracker(tracker_url);
}

bool TorrentTracker::IsUdpTracker(const std::string& url) const {
    return url.substr(0, 6) == "udp://";
}

std::pair<std::string, int> TorrentTracker::ParseUdpUrl(const std::string& url) {
    if (url.substr(0, 6) != "udp://") {
        throw std::runtime_error("Invalid UDP URL: " + url);
    }

    std::string host_port = url.substr(6);

    size_t slash_pos = host_port.find('/');
    if (slash_pos != std::string::npos) {
        host_port = host_port.substr(0, slash_pos);
    }

    size_t colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        std::string host = host_port.substr(0, colon_pos);
        int port = std::stoi(host_port.substr(colon_pos + 1));
        return {host, port};
    } else {
        return {host_port, 80};
    }
}

Peer TorrentTracker::ConvertTrackerPeer(const UdpTracker::TrackerPeer& tracker_peer) {
    Peer peer;

    peer.ip = std::to_string((tracker_peer.ip >> 24) & 0xFF) + "." +
              std::to_string((tracker_peer.ip >> 16) & 0xFF) + "." +
              std::to_string((tracker_peer.ip >> 8) & 0xFF) + "." +
              std::to_string(tracker_peer.ip & 0xFF);

    peer.port = tracker_peer.port;

    return peer;
}

void TorrentTracker::UpdatePeersHttp(const TorrentFile& torrent_file,
                                     const std::string& peer_id,
                                     int port,
                                     const std::string& url) {
    cpr::Response tracker_response = cpr::Get(
        cpr::Url{url},
        cpr::Parameters{
            {"info_hash", torrent_file.info_hash},
            {"peer_id", peer_id},
            {"port", std::to_string(port)},
            {"uploaded", "0"},
            {"downloaded", "0"},
            {"left", std::to_string(torrent_file.length)},
            {"compact", "1"}
        },
        cpr::Timeout{10000},
        cpr::ConnectTimeout{5000}
    );

    if (tracker_response.status_code != 200) {
        throw std::runtime_error("HTTP " + std::to_string(tracker_response.status_code) +
                               ": " + tracker_response.error.message);
    }

    ParseTrackerResponse(tracker_response.text, url);
}

void TorrentTracker::UpdatePeersUdp(const TorrentFile& torrent_file,
                                    const std::string& peer_id,
                                    int port,
                                    const std::string& url) {
    try {
        auto [host, tracker_port] = ParseUdpUrl(url);

        UdpTracker udp_tracker(host, tracker_port, 8);

        auto response = udp_tracker.Announce(
            torrent_file.info_hash,  // info_hash (20 bytes)
            peer_id,                 // peer_id (20 bytes)
            0,                       // downloaded
            torrent_file.length,     // left
            0,                       // uploaded
            2,                       // event: 2 = started
            -1,                      // num_want: -1 = default
            port                     // port
        );

        peers.clear();
        for (const auto& tracker_peer : response.peers) {
            peers.push_back(ConvertTrackerPeer(tracker_peer));
        }

    } catch (const std::exception& e) {
        throw;
    }
}

void TorrentTracker::ParseTrackerResponse(const std::string& response) {
    ParseTrackerResponse(response, tracker_url);
}

void TorrentTracker::ParseTrackerResponse(const std::string& response, const std::string& url) {
    utils::BencodeParser parser;
    auto parsed = parser.ParseFromString(response);

    std::string peers_data;
    std::string failure_reason;

    for (size_t i = 0; i < parsed.size(); ++i) {
        if (parsed[i] == "peers" && i + 1 < parsed.size()) {
            peers_data = parsed[i + 1];
        }
        if (parsed[i] == "failure reason" && i + 1 < parsed.size()) {
            failure_reason = parsed[i + 1];
        }
        if (parsed[i] == "interval" && i + 1 < parsed.size()) {
            // do nothing
        }
    }

    if (!failure_reason.empty()) {
        throw std::runtime_error("Tracker failure: " + failure_reason);
    }

    if (peers_data.empty()) {
        throw std::runtime_error("No peers data in tracker response from " + url);
    }

    ParseCompactPeers(peers_data);
}

void TorrentTracker::ParseCompactPeers(const std::string& peers_data) {
    peers.clear();

    if (peers_data.size() % 6 == 0 && !peers_data.empty()) {
        ParseCompactBinaryPeers(peers_data);
    } else {
        ParseDictionaryPeers(peers_data);
    }
}

void TorrentTracker::ParseCompactBinaryPeers(const std::string& peers_data) {
    static constexpr size_t kPeerSize = 6;
    const size_t peer_count = peers_data.size() / kPeerSize;
    peers.reserve(peer_count);

    for (size_t i = 0; i < peers_data.size(); i += kPeerSize) {
        std::string ip = std::to_string(static_cast<uint8_t>(peers_data[i])) + "." +
                        std::to_string(static_cast<uint8_t>(peers_data[i + 1])) + "." +
                        std::to_string(static_cast<uint8_t>(peers_data[i + 2])) + "." +
                        std::to_string(static_cast<uint8_t>(peers_data[i + 3]));

        int port = (static_cast<uint8_t>(peers_data[i + 4]) << 8) |
                   static_cast<uint8_t>(peers_data[i + 5]);

        peers.emplace_back(Peer{ip, port});
    }
}

void TorrentTracker::ParseDictionaryPeers(const std::string& peers_data) {
    static_cast<void>(peers_data);
    throw std::runtime_error("Non-compact peer format not supported. Use compact=1 in tracker request.");
}

const std::vector<Peer>& TorrentTracker::GetPeers() const {
    return peers;
}

std::string TorrentTracker::GetTrackerUrl() const {
    return tracker_url;
}

bool TorrentTracker::IsWorking() const {
    return !peers.empty();
}

void TorrentTracker::PrintStats() const {

}
