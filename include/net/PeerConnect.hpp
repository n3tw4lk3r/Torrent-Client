#pragma once

#include "net/Peer.hpp"
#include "core/TorrentFile.hpp"
#include "core/PieceStorage.hpp"
#include "net/TcpConnect.hpp"
#include <atomic>

class PeerPiecesAvailability {
public:
    PeerPiecesAvailability() = default;
    PeerPiecesAvailability(std::string bitfield, size_t size);

    bool IsPieceAvailable(size_t piece_index) const;
    void SetPieceAvailability(size_t pieceIndex);

private:
    std::string bitfield;
    size_t size = 0;
};

class PeerConnect {
public:
    PeerConnect(const Peer& peer,
                const TorrentFile& torrent_file,
                std::string self_peer_id,
                PieceStorage& piece_storage);

    void Run();
    void Terminate();
    bool IsTerminated() const;
    std::string GetPeerId() const;
    bool Failed() const;

private:
    bool EstablishConnection();
    void PerformHandshake();
    void ReceiveBitfield();
    void SendInterested();
    void MainLoop();
    void ProcessMessage(const std::string& message_data);
    void RequestPiece(const Block* block);
    void HandleConnectionError();
    PiecePtr GetNextAvailablePiece();

    TorrentFile torrent_file;
    TcpConnect socket;
    std::string self_peer_id;
    std::string peer_id;

    PeerPiecesAvailability pieces_availability;
    PieceStorage& piece_storage;

    PiecePtr piece_is_in_progress;
    bool block_is_pending = false;
    bool is_choked = true;
    std::atomic<bool> is_terminated = false;
    bool has_failed = false;
};
