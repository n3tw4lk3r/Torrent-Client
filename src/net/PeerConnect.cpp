#include "net/PeerConnect.hpp"
#include "net/Message.hpp"
#include "utils/byte_tools.hpp"
#include <thread>

using namespace std::chrono_literals;

PeerPiecesAvailability::PeerPiecesAvailability(std::string bitfield, size_t size)
    : bitfield(std::move(bitfield)), size(size) {}

bool PeerPiecesAvailability::IsPieceAvailable(size_t index) const {
    if (index >= size * 8) return false;
    return (bitfield[index >> 3] >> (7 - (index & 7))) & 1;
}

void PeerPiecesAvailability::SetPieceAvailability(size_t index) {
    if (index < size * 8)
        bitfield[index >> 3] |= (1 << (7 - (index & 7)));
}

PeerConnect::PeerConnect(const Peer& peer,
                         const TorrentFile& torrent_file,
                         std::string self_peer_id,
                         PieceStorage& piece_storage)
    : torrent_file(torrent_file),
      socket(peer.ip, peer.port, 3500ms, 3500ms),
      self_peer_id(std::move(self_peer_id)),
      piece_storage(piece_storage) {}

void PeerConnect::Run() {
    int failures = 0;

    while (!is_terminated && failures < 10) {
        try {
            if (EstablishConnection()) {
                failures = 0;
                MainLoop();
            } else {
                ++failures;
            }
        } catch (...) {
            ++failures;
            HandleConnectionError();
        }
    }

    Terminate();
}

bool PeerConnect::EstablishConnection() {
    socket.EstablishConnection();
    PerformHandshake();
    ReceiveBitfield();
    SendInterested();
    return true;
}

void PeerConnect::PerformHandshake() {
    std::string msg;
    msg += char(19);
    msg += "BitTorrent protocol";
    msg += std::string(8, '\0');
    msg += torrent_file.info_hash;
    msg += self_peer_id;

    socket.SendData(msg);
    auto resp = socket.ReceiveData(68);
    peer_id = resp.substr(48, 20);
}

void PeerConnect::ReceiveBitfield() {
    auto data = socket.ReceiveData();
    if (data.size() < 5) return;

    if (data[4] == static_cast<char>(MessageId::kBitField)) {
        auto bf = data.substr(5);
        pieces_availability = PeerPiecesAvailability(
            bf, (torrent_file.piece_hashes.size() + 7) / 8
        );
    }
}

void PeerConnect::SendInterested() {
    socket.SendData(Message::Init(MessageId::kInterested, "").ToString());
}

void PeerConnect::MainLoop() {
    while (!is_terminated) {
        if (!piece_is_in_progress)
            piece_is_in_progress = GetNextAvailablePiece();

        if (!piece_is_in_progress) {
            std::this_thread::sleep_for(50ms);
            continue;
        }

        if (!is_choked && !block_is_pending) {
            auto block = piece_is_in_progress->GetFirstMissingBlock();
            if (block) {
                RequestPiece(block);
                block_is_pending = true;
            }
        }

        auto msg = socket.ReceiveData();
        if (!msg.empty())
            ProcessMessage(msg);
    }
}

PiecePtr PeerConnect::GetNextAvailablePiece() {
    while (!is_terminated) {
        auto piece = piece_storage.GetNextPieceToDownload();
        if (!piece) return nullptr;

        if (pieces_availability.IsPieceAvailable(piece->GetIndex()))
            return piece;

        piece_storage.Enqueue(piece);
    }
    return nullptr;
}

void PeerConnect::ProcessMessage(const std::string& data) {
    auto msg = Message::Parse(data);

    if (msg.id == MessageId::kUnchoke)
        is_choked = false;

    if (msg.id == MessageId::kPiece) {
        size_t index = utils::BytesToInt(msg.payload.substr(0, 4));
        size_t offset = utils::BytesToInt(msg.payload.substr(4, 4));
        auto block = msg.payload.substr(8);

        if (piece_is_in_progress && piece_is_in_progress->GetIndex() == index) {
            piece_is_in_progress->SaveBlock(offset, block);
            block_is_pending = false;

            if (piece_is_in_progress->AllBlocksRetrieved()) {
                piece_storage.PieceProcessed(piece_is_in_progress);
                piece_is_in_progress.reset();
            }
        }
    }
}

void PeerConnect::RequestPiece(const Block* block) {
    std::string payload;
    payload += utils::IntToBytes(block->piece);
    payload += utils::IntToBytes(block->offset);
    payload += utils::IntToBytes(block->length);
    socket.SendData(Message::Init(MessageId::kRequest, payload).ToString());
}

void PeerConnect::HandleConnectionError() {
    if (piece_is_in_progress) {
        piece_storage.Enqueue(piece_is_in_progress);
        piece_is_in_progress.reset();
    }
}

void PeerConnect::Terminate() {
    is_terminated = true;
    socket.CloseConnection();
}

bool PeerConnect::IsTerminated() const {
    return is_terminated;
}

std::string PeerConnect::GetPeerId() const {
    return peer_id;
}

bool PeerConnect::Failed() const {
    return has_failed;
}
