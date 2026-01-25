#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/UdpConnection.hpp"

class UdpTracker {
public:
    struct TrackerPeer {
        uint32_t ip;
        uint16_t port;
    };

    struct TrackerResponse {
        uint32_t interval;
        uint32_t leechers;
        uint32_t seeders;
        std::vector<TrackerPeer> peers;
    };

    UdpTracker(const std::string& host, int port, int timeout_sec = 5);

    TrackerResponse Announce(const std::string& info_hash,
                             const std::string& peer_id,
                             uint64_t downloaded,
                             uint64_t left,
                             uint64_t uploaded,
                             int event,
                             int wanted_number,
                             uint16_t port);

private:
    std::string host;
    int port;
    int timeout_sec;

    UdpConnection udp_client;

    uint64_t Connect();

    TrackerResponse AnnounceWithConnection(uint64_t connection_id,
                                           const std::string& info_hash,
                                           const std::string& peer_id,
                                           uint64_t downloaded,
                                           uint64_t left,
                                           uint64_t uploaded,
                                           int event,
                                           int wanted_number,
                                           uint16_t port);
};
