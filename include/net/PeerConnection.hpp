#pragma once

#include <atomic>
#include <unordered_set>

#include "core/PieceStorage.hpp"
#include "core/TorrentFile.hpp"
#include "net/Peer.hpp"
#include "net/TcpConnection.hpp"

class PeerConnection {
public:
    PeerConnection(const Peer& peer,
                   const TorrentFile& torrent_file,
                   std::string self_peer_id,
                   PieceStorage& piece_storage);

    void Run();
    void Terminate();
    bool IsTerminated() const;
    std::string GetPeerId() const;
    bool Failed() const;

private:
    class PeerPiecesAvailability {
    public:
        PeerPiecesAvailability() = default;
        PeerPiecesAvailability(std::string bitfield, size_t size);

        bool IsPieceAvailable(size_t piece_index) const;
        void SetPieceAvailability(size_t piece_index);

    private:
        std::string bitfield;
        size_t size = 0;
    };

    bool EstablishConnection();
    void PerformHandshake();
    void ReceiveBitfield();
    void SendInterested();
    void MainLoop();
    void ProcessMessage(const std::string& message_data);
    void RequestBlock(const Block* block);
    void HandleConnectionError();
    PiecePtr GetNextAvailablePiece();

    static constexpr int kMaxInflightBlocks = 16;

    TorrentFile torrent_file;
    TcpConnection socket;
    std::string self_peer_id;
    std::string peer_id;

    PeerPiecesAvailability pieces_availability;
    PieceStorage& piece_storage;

    PiecePtr piece_in_progress;
    std::unordered_set<size_t> inflight_offsets;

    bool is_choked = true;
    std::atomic<bool> is_terminated = false;
    bool has_failed = false;
};
