#pragma once

#include <array>
#include <string>
#include <vector>

#include "core/TorrentFile.hpp"
#include "core/UdpTracker.hpp"
#include "net/Peer.hpp"

class TorrentTracker {
public:
    explicit TorrentTracker(const std::string& url);

    void UpdatePeers(const TorrentFile& torrent_file,
                     const std::string& peer_id,
                     int port);

    const std::vector<Peer>& GetPeers() const;
    std::string GetTrackerUrl() const;
    bool IsWorking() const;
    void PrintStats() const;
    void SetPeers(const std::vector<Peer>& new_peers) { peers = new_peers; }

private:
    static constexpr std::array<std::string_view, 7> kBackupUdpTrackers = {
        "udp://tracker.openbittorrent.com:80",
        "udp://tracker.internetwarriors.net:1337",
        "udp://tracker.leechers-paradise.org:6969",
        "udp://tracker.coppersurfer.tk:6969",
        "udp://open.stealth.si:80",
        "udp://exodus.desync.com:6969",
        "udp://tracker.torrent.eu.org:451"
    };

    bool IsUdpTracker() const;
    bool IsUdpTracker(const std::string& url) const;
    std::pair<std::string, int> ParseUdpUrl(const std::string& url);
    Peer ConvertTrackerPeer(const UdpTracker::TrackerPeer& tracker_peer);
    void UpdatePeersHttp(const TorrentFile& torrent_file,
                         const std::string& peer_id,
                         int port,
                         const std::string& url);
    void UpdatePeersUdp(const TorrentFile& torrent_file,
                        const std::string& peer_id,
                        int port,
                        const std::string& url);
    void ParseTrackerResponse(const std::string& response);
    void ParseTrackerResponse(const std::string& response, const std::string& url);
    void ParseCompactPeers(const std::string& peers_data);
    void ParseCompactBinaryPeers(const std::string& peers_data);
    void ParseDictionaryPeers(const std::string& peers_data);

    std::string tracker_url;
    std::vector<Peer> peers;
};
