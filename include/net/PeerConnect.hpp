#pragma once

#include "net/TcpConnect.hpp"
#include "net/Peer.hpp"
#include "core/TorrentFile.hpp"
#include "core/PieceStorage.hpp"
#include <atomic>
#include <string>

class PeerPiecesAvailability {
public:
    PeerPiecesAvailability() = default;
    explicit PeerPiecesAvailability(std::string bitfield, size_t size);
    bool IsPieceAvailable(size_t piece_index) const;
    void SetPieceAvailability(size_t piece_index);
    size_t Size() const;

private:
    std::string bitfield;
    size_t size;
};

class PeerConnect {
public:
    PeerConnect(const Peer& peer, const TorrentFile& torrent_file, std::string self_peer_id, PieceStorage& piece_storage);
    ~PeerConnect() = default;

    void HandleConnectionError();
    void Run();
    void Terminate();
    bool Failed() const;
    bool IsDownloading() const;
    bool IsTerminated() const;
    std::string GetPeerId() const;

private:
    const TorrentFile& torrent_file;
    TcpConnect socket;
    const std::string self_peer_id;
    std::string peer_id;
    PeerPiecesAvailability pieces_availability;
    std::atomic<bool> is_terminated = false;
    bool is_choked = true;
    PiecePtr piece_is_in_progress;
    PieceStorage& piece_storage;
    bool block_is_pending = false;
    bool has_failed = false;

    void PerformHandshake();
    bool EstablishConnection();
    void ReceiveBitfield();
    void SendInterested();
    void RequestPiece(const Block* block);
    void MainLoop();
    PiecePtr GetNextAvailablePiece();
    void ProcessMessage(const std::string& messageData);
};
