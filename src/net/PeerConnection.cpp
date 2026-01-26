#include "net/PeerConnection.hpp"

#include <thread>

#include "net/Message.hpp"
#include "utils/byte_tools.hpp"

using namespace std::chrono_literals;

PeerConnection::PeerPiecesAvailability::PeerPiecesAvailability(std::string bitfield, size_t size) :
    bitfield(std::move(bitfield)),
    size(size)
{}

bool PeerConnection::PeerPiecesAvailability::IsPieceAvailable(size_t index) const {
    if (index >= size * 8) {
        return false;
    }
    return (bitfield[index >> 3] >> (7 - (index & 7))) & 1;
}

void PeerConnection::PeerPiecesAvailability::SetPieceAvailability(size_t index) {
    if (index < size * 8) {
        bitfield[index >> 3] |= (1 << (7 - (index & 7)));
    }
}

PeerConnection::PeerConnection(const Peer& peer,
                               const TorrentFile& torrent_file,
                               std::string self_peer_id,
                               PieceStorage& piece_storage)
    : torrent_file(torrent_file),
      socket(peer.ip, peer.port, 3500ms, 3500ms),
      self_peer_id(std::move(self_peer_id)),
      piece_storage(piece_storage) {}

void PeerConnection::Run() {
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

bool PeerConnection::EstablishConnection() {
    socket.EstablishConnection();
    PerformHandshake();
    ReceiveBitfield();
    SendInterested();
    return true;
}

void PeerConnection::PerformHandshake() {
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

void PeerConnection::ReceiveBitfield() {
    auto data = socket.ReceiveData();
    if (data.size() < 5) {
        return;
    }

    if (data[4] == static_cast<char>(MessageId::kBitField)) {
        auto bf = data.substr(5);
        pieces_availability = PeerPiecesAvailability(
            bf, (torrent_file.piece_hashes.size() + 7) / 8
        );
    }
}

void PeerConnection::SendInterested() {
    socket.SendData(Message::Init(MessageId::kInterested, "").ToString());
}

void PeerConnection::MainLoop() {
    while (!is_terminated) {
        if (!piece_in_progress) {
            piece_in_progress = GetNextAvailablePiece();
            inflight_offsets.clear();
        }

        if (!piece_in_progress) {
            std::this_thread::sleep_for(5ms);
            continue;
        }

        while (!is_choked && inflight_offsets.size() < kMaxInflightBlocks) {
            auto block = piece_in_progress->GetFirstMissingBlock();
            if (!block) {
                break;
            }

            if (inflight_offsets.contains(block->offset)) {
                break;
            }

            RequestBlock(block);
            inflight_offsets.insert(block->offset);
        }

        auto msg = socket.ReceiveData();
        if (!msg.empty()) {
            ProcessMessage(msg);
        }
    }
}

PiecePtr PeerConnection::GetNextAvailablePiece() {
    while (!is_terminated) {
        auto piece = piece_storage.GetNextPieceToDownload();
        if (!piece) {
            return nullptr;
        }

        if (pieces_availability.IsPieceAvailable(piece->GetIndex())) {
            return piece;
        }

        piece_storage.Enqueue(piece);
    }
    return nullptr;
}

void PeerConnection::ProcessMessage(const std::string& data) {
    auto msg = Message::Parse(data);

    switch (msg.id) {
        case MessageId::kUnchoke:
            is_choked = false;
            break;

        case MessageId::kChoke:
            is_choked = true;
            inflight_offsets.clear();
            break;

        case MessageId::kHave: {
            size_t index = utils::BytesToInt32(msg.payload);
            pieces_availability.SetPieceAvailability(index);
            break;
        }

        case MessageId::kPiece: {
            size_t index = utils::BytesToInt32(msg.payload.substr(0, 4));
            size_t offset = utils::BytesToInt32(msg.payload.substr(4, 4));
            auto block = msg.payload.substr(8);

            if (piece_in_progress && piece_in_progress->GetIndex() == index) {

                piece_in_progress->SaveBlock(offset, block);
                inflight_offsets.erase(offset);

                if (piece_in_progress->AllBlocksRetrieved()) {
                    piece_storage.PieceProcessed(piece_in_progress);
                    piece_in_progress.reset();
                    inflight_offsets.clear();
                }
            }
            break;
        }

        default:
            break;
    }
}

void PeerConnection::RequestBlock(const Block* block) {
    std::string payload;
    payload += utils::Int32ToBytes(block->piece);
    payload += utils::Int32ToBytes(block->offset);
    payload += utils::Int32ToBytes(block->length);
    socket.SendData(Message::Init(MessageId::kRequest, payload).ToString());
}

void PeerConnection::HandleConnectionError() {
    if (piece_in_progress) {
        piece_storage.Enqueue(piece_in_progress);
        piece_in_progress.reset();
    }
}

void PeerConnection::Terminate() {
    is_terminated = true;
    socket.CloseConnection();
}

bool PeerConnection::IsTerminated() const {
    return is_terminated;
}

std::string PeerConnection::GetPeerId() const {
    return peer_id;
}

bool PeerConnection::Failed() const {
    return has_failed;
}
