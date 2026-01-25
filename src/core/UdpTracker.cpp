#include "core/UdpTracker.hpp"

#include <cstring>
#include <stdexcept>

#include "utils/byte_tools.hpp"

UdpTracker::UdpTracker(const std::string& host, int port, int timeout_sec) :
    host(host),
    port(port),
    timeout_sec(timeout_sec),
    udp_client(host, port, timeout_sec)
{}

UdpTracker::TrackerResponse UdpTracker::Announce(
    const std::string& info_hash,
    const std::string& peer_id,
    uint64_t downloaded,
    uint64_t left,
    uint64_t uploaded,
    int event,
    int num_want,
    uint16_t port) {
    uint64_t connection_id = Connect();
    return AnnounceWithConnection(connection_id, info_hash, peer_id, downloaded, left,
                                  uploaded, event, num_want, port);
}

uint64_t UdpTracker::Connect() {
    uint64_t protocol_id = 0x41727101980;
    uint32_t action = 0; // connect = 0
    uint32_t transaction_id = utils::BytesToInt32(utils::Int32ToBytes(rand()));

    std::string request;
    request.reserve(16);

    request += utils::Int64ToBytes(protocol_id);
    request += utils::Int32ToBytes(action);
    request += utils::Int32ToBytes(transaction_id);

    std::string response = udp_client.SendReceive(request);

    if (response.size() < 16) {
        throw std::runtime_error("CONNECT response too small: " + std::to_string(response.size()));
    }

    uint32_t resp_action = utils::BytesToInt32(response.substr(0, 4));
    uint32_t resp_trans = utils::BytesToInt32(response.substr(4, 4));

    if (resp_action != 0) {
        throw std::runtime_error("CONNECT failed: action=" + std::to_string(resp_action));
    }

    if (resp_trans != transaction_id) {
        throw std::runtime_error("CONNECT transaction_id mismatch");
    }

    uint64_t connection_id = utils::BytesToInt64(response.substr(8, 8));

    return connection_id;
}

UdpTracker::TrackerResponse UdpTracker::AnnounceWithConnection(
    uint64_t connection_id,
    const std::string& info_hash,
    const std::string& peer_id,
    uint64_t downloaded,
    uint64_t left,
    uint64_t uploaded,
    int event,
    int num_want,
    uint16_t port) {
    if (info_hash.size() != 20) {
        throw std::runtime_error("info_hash must be 20 bytes");
    }
    if (peer_id.size() != 20) {
        throw std::runtime_error("peer_id must be 20 bytes");
    }

    uint32_t action = 1; // announce
    uint32_t transaction_id = utils::BytesToInt32(utils::Int32ToBytes(rand()));

    std::string request;
    request.reserve(98);

    request += utils::Int64ToBytes(connection_id);
    request += utils::Int32ToBytes(action);
    request += utils::Int32ToBytes(transaction_id);
    request += info_hash;
    request += peer_id;
    request += utils::Int64ToBytes(downloaded);
    request += utils::Int64ToBytes(left);
    request += utils::Int64ToBytes(uploaded);
    request += utils::Int32ToBytes(event); // 0=none,1=completed,2=started,3=stopped
    request += utils::Int32ToBytes(0);     // IP = default
    request += utils::Int32ToBytes(rand()); // key
    request += utils::Int32ToBytes(num_want);
    request += utils::Int32ToBytes(port << 16 | (port & 0xFFFF));

    std::string response = udp_client.SendReceive(request);

    if (response.size() < 20) {
        throw std::runtime_error("ANNOUNCE response too small: " + std::to_string(response.size()));
    }

    uint32_t resp_action = utils::BytesToInt32(response.substr(0, 4));
    uint32_t resp_trans  = utils::BytesToInt32(response.substr(4, 4));

    if (resp_action == 3) { // error
        std::string err_msg = response.substr(8);
        throw std::runtime_error("Tracker error: " + err_msg);
    }

    if (resp_action != 1) {
        throw std::runtime_error("ANNOUNCE failed: wrong action=" + std::to_string(resp_action));
    }

    if (resp_trans != transaction_id) {
        throw std::runtime_error("ANNOUNCE transaction_id mismatch");
    }

    TrackerResponse tracker_response;
    tracker_response.interval = utils::BytesToInt32(response.substr(8, 4));
    tracker_response.leechers = utils::BytesToInt32(response.substr(12, 4));
    tracker_response.seeders  = utils::BytesToInt32(response.substr(16, 4));

    size_t offset = 20;
    while (offset + 6 <= response.size()) {
        TrackerPeer peer;

        peer.ip   = utils::BytesToInt32(response.substr(offset, 4));
        peer.port = (uint16_t)((static_cast<unsigned char>(response[offset + 4]) << 8) |
                               static_cast<unsigned char>(response[offset + 5]));

        tracker_response.peers.push_back(peer);
        offset += 6;
    }

    return tracker_response;
}
