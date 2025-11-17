#include "byte_tools.h"
#include "peer_connect.h"
#include "message.h"
#include <thread>

using namespace std::chrono_literals;

PeerPiecesAvailability::PeerPiecesAvailability(std::string bitfield, size_t size)
    : bitfield_(bitfield), size_(size) {}

bool PeerPiecesAvailability::IsPieceAvailable(size_t pieceIndex) const {
    std::cout << "bietfield: isPieceAvailable\n";
    return (bitfield_[pieceIndex / 8] >> (7 - pieceIndex % 8) & 1) != 0;
}

void PeerPiecesAvailability::SetPieceAvailability(size_t pieceIndex) {
    std::cout << "bietfield: SetPieceAvailability\n";
    bitfield_[pieceIndex / 8] |= (1 << (7 - pieceIndex % 8));
}

size_t PeerPiecesAvailability::Size() const {
    std::cout << "bietfield: Size\n";
    return size_; // https://stackoverflow.com/questions/44308457/confusion-around-bitfield-torrent
}

PeerConnect::PeerConnect(const Peer& peer, const TorrentFile &tf, std::string selfPeerId, PieceStorage& pieceStorage)
    : socket_(peer.ip, peer.port, 10000ms, 10000ms)
    , tf_(tf)
    , selfPeerId_(selfPeerId)
    , pieceStorage_(pieceStorage) {}

void PeerConnect::Run() {
    while (!terminated_) {
        if (EstablishConnection()) {
            // std::cout << "Connection established to peer" << std::endl;
            MainLoop();
        } else {
            // std::cerr << "Cannot establish connection to peer" << std::endl;
            failed_ = true;
            Terminate();
            throw std::runtime_error("shit happened");
        }
    }
}

void PeerConnect::PerformHandshake() {
    // std::cout << "peer_connect: Shaking hands...\n";
    unsigned char pstrln = (char) 19;
    std::string pstrlen;
    pstrlen += pstrln;
    std::string pstr = "BitTorrent protocol";
    std::string reserved;
    for (int i = 0; i < 8; ++i) {
        reserved += '\0';
    }

    std::string handshakeStr = pstrlen + pstr + reserved + tf_.infoHash + selfPeerId_;
    socket_.SendData(handshakeStr);
    std::string response = socket_.ReceiveData(68);

    if (response.substr(28, 20) != tf_.infoHash) {
        failed_ = true;
        Terminate();
        throw std::runtime_error("Peer sent mismatching infohash");
    }
    // std::cout << "Handshake completed!\n";
}

bool PeerConnect::EstablishConnection() {
    try {
        socket_.EstablishConnection();
        PerformHandshake();
        ReceiveBitfield();
        SendInterested();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to establish connection with peer " << socket_.GetIp() << ":" <<
            socket_.GetPort() << " -- " << e.what() << std::endl;
            failed_ = true;
            Terminate();
        return false;
    }
}

void PeerConnect::ReceiveBitfield() {
    // std::cout << "receiving bitfield...\n";
    std::string msg = socket_.ReceiveData();

    int sz = BytesToInt(msg.substr(0, 4));
    if (sz == 0) {
        // std::cout << "Keep-alive\n";
        return;
    }

    int id = (char) msg[4];
    if (id == 1) {
        // std::cout << "Unchoked!!!\n";
        choked_ = false;
        return;
    }

    // std::cout << "not unchoked yet...\n";
    std::string bitfield = msg.substr(5);
    // https://stackoverflow.com/questions/44308457/confusion-around-bitfield-torrent
    piecesAvailability_ = PeerPiecesAvailability(bitfield, ceil(tf_.pieceHashes.size() / 8));
}

void PeerConnect::SendInterested() {
    // std::cout << "sending interested...\n";
    std::string len = IntToBytes(1);
    unsigned char ch = 2 & 0xFF;
    std::string id;
    id += ch;
    socket_.SendData(len + id);
}

void PeerConnect::Terminate() {
    // std::cerr << "Terminate" << std::endl;
    terminated_ = true;
}

void PeerConnect::MainLoop() {
    if (pieceStorage_.QueueIsEmpty()) {
        std::cout << "ML: queue is empty\n";
        return;
    }

    PiecePtr currentPiece = pieceStorage_.GetNextPieceToDownload();
    std::cout << "trying to download piece " << currentPiece->GetIndex() << '\n';

    while (!terminated_) {
        if (!currentPiece) {
            if (pieceStorage_.QueueIsEmpty()) {
                std::cout << "Download finished.\n";
                Terminate();
                break;
            }

            currentPiece = pieceStorage_.GetNextPieceToDownload();
            std::cout << "picked new piece...\n";
        }

        try {
            std::string recvd = socket_.ReceiveData();
            Message msg = Message::Parse(recvd);
            std::string payload = msg.payload;

            if (msg.id == MessageId::Choke) {
                // std::cout << "Received choke\n";
                choked_ = true;
            }

            if (msg.id == MessageId::Unchoke) {
                // std::cout << "Received unchoke\n";
                choked_ = false;
            }

            if (msg.id == MessageId::Have) {
                // std::cout << "Received have\n";
                size_t idx = BytesToInt(msg.payload);
                piecesAvailability_.SetPieceAvailability(idx);
            }

            if (msg.id == MessageId::Piece) {
                // std::cout << "Received piece\n";
                int idx = BytesToInt(payload.substr(0, 4));
                int begin = BytesToInt(payload.substr(4, 4));
                std::string data = payload.substr(8);
                currentPiece->SaveBlock(begin, data);
                pendingBlock_ = false;

                if (currentPiece->AllBlocksRetrieved()) {
                    std::cout << "fully downloaded piece\n";
                    pieceStorage_.PieceProcessed(currentPiece);
                    if (pieceStorage_.QueueIsEmpty()) {
                        std::cout << "Download finished.\n";
                        Terminate();
                        break;
                    }

                    piecesAvailability_.SetPieceAvailability(currentPiece->GetIndex());
                    std::cout << "picking new piece...\n";
                    currentPiece = pieceStorage_.GetNextPieceToDownload();
                }
            }

            if (msg.id == MessageId::KeepAlive) {
                std::cout << "Received keep-alive\n";
                continue;
            }

            if (!choked_ && !pendingBlock_) {
                RequestPiece(currentPiece->FirstMissingBlock());
                pendingBlock_ = true;
            }
        }

        catch (const std::exception& e) {
            std::cout << "something went wrong, putting piece" << currentPiece->GetIndex() << " back to queue -- " << e.what() << std::endl;
            currentPiece.reset();
            pieceStorage_.Enqueue(currentPiece);
            Terminate();
            failed_ = true;
        }
    }
    
}

void PeerConnect::RequestPiece(const Block* block) {
    // request: <len=0013><id=6><index><begin><length>
    if (!block) {
        return;
    }
    std::string msg;
    msg += IntToBytes(13);
    msg += (unsigned char) 6;
    uint32_t index = block->piece;
    msg += IntToBytes(index);
    uint32_t offset = block->offset;
    msg += IntToBytes(offset);
    uint32_t length = block->length;
    msg += IntToBytes(length);

    // std::cout << "--- requesting piece " << block->piece << " offset " << block->offset << " ---\n";
    socket_.SendData(msg);
}

bool PeerConnect::Failed() const {
    return failed_;
}

