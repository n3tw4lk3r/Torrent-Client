#include "tcp_connect.h"
#include "byte_tools.h"

TcpConnect::TcpConnect(std::string ip, int port, std::chrono::milliseconds connectTimeout, std::chrono::milliseconds readTimeout)
    : ip_(ip)
    , port_(port)
    , connectTimeout_(connectTimeout)
    , readTimeout_(readTimeout)
    { sock_ = socket(AF_INET, SOCK_STREAM, 0); }

TcpConnect::~TcpConnect() {
    close(sock_);
}

void TcpConnect::EstablishConnection() {
    // std::cout << "tcp_connect: Trying to connect " << ip_ << '\n';

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip_.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port_);

    fd_set fdst;
    struct timeval tmvl;

    int curState = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, curState | O_NONBLOCK);
    int code = connect(sock_, (struct sockaddr*) &server, sizeof(struct sockaddr_in));
    if (code == 0) {
        curState = fcntl(sock_, F_GETFL, 0);
        fcntl(sock_, F_SETFL, curState & ~O_NONBLOCK);
        // std::cout << "Connected\n";
        return;
    }

    FD_ZERO(&fdst);
    FD_SET(sock_, &fdst);
    tmvl.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(connectTimeout_).count();
    tmvl.tv_usec = 0;
    
    int cd = select(sock_ + 1, NULL, &fdst, NULL, &tmvl);
    
    switch (cd) {
        case 0:
            close(sock_);
            throw std::runtime_error("Connection timeout");
            break;

        case 1:
        default:
            int soError;
            socklen_t ln = sizeof soError;
            getsockopt(sock_, SOL_SOCKET, SO_ERROR, &soError, &ln);

            if (soError == 0) {
                curState = fcntl(sock_, F_GETFL, 0);
                fcntl(sock_, F_SETFL, curState & ~O_NONBLOCK);
                // std::cout << "Connected\n";
                return;
            }

            // std::cout << "Connect error: " << strerror(errno) << ' ' << errno << '\n';
            close(sock_);
            throw std::runtime_error("Socket connection error");
    }
}

void TcpConnect::SendData(const std::string& data) const {
    // std::cout << "tcp_connect: Sending data: " << data.size() << " bytes\n";
    char toSend[data.size()];
    for (int i = 0; i < data.size(); ++i) {
        toSend[i] = data[i];
    }

    if ((send(sock_, toSend, data.size(), 0)) < 0) {
        std::cout << "Send error: " << strerror(errno) << ' ' << errno << '\n';
        close(sock_);
        throw std::runtime_error("Send error");
    }
    // std::cout << "Sent ok.\n";
}

std::string TcpConnect::ReceiveData(size_t bufferSize) const {
    // std::cout << "tcp_connect: Receiving data...\n";
    std::string msg;

    if (!bufferSize) {
        // std::cout << "Amount unknown.\n";
        struct pollfd fd;
        fd.fd = sock_;
        fd.events = POLLIN;
        int cd = poll(&fd, 1, readTimeout_.count());
        char data[4];

        switch (cd) {
            case -1:
                // std::cout << "Connect error: " << strerror(errno) << ' ' << errno << '\n';
                close(sock_);
                throw std::runtime_error("Shit happened");

            case 0:
                close(sock_);
                throw std::runtime_error("Read timeout");

            default:
                recv(sock_, data, sizeof(data), 0);
        }

        for (char& ch : data) {
            msg += ch;
        }
        bufferSize = BytesToInt(msg);
    }

    // std::cout << "Left to receive: " << bufferSize << '\n';
    if (bufferSize > 100'000) {
        close(sock_);
        throw std::runtime_error("...wtf, too much data\n");
    }

    int toRead = bufferSize;
    char data1[bufferSize];
    while (toRead) {
        int read = recv(sock_, data1, sizeof(data1), 0);
        if (read <= 0) {
            close(sock_);
            throw std::runtime_error("read error");
        }
        for (int i = 0; i < read; ++i) {
            msg += data1[i];
        }
        toRead -= read;
    }

    // std::cout << "Total received: " << msg.size() << '\n';
    return msg;
}

void TcpConnect::CloseConnection() {
    close(sock_);
}

const std::string &TcpConnect::GetIp() const {
    return ip_;
}

int TcpConnect::GetPort() const {
    return port_;
}
