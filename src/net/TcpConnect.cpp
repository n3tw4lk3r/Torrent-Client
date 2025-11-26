#include "net/TcpConnect.hpp"
#include "utils/byte_tools.hpp"
#include <cstdio>

TcpConnect::TcpConnect(std::string ip, int port,
                       std::chrono::milliseconds connect_timeout,
                       std::chrono::milliseconds read_timeout)
    : ip(ip), port(port),
      connect_timeout(connect_timeout),
      read_timeout(read_timeout),
      force_close_(false) {
    sock = -1;
}

TcpConnect::~TcpConnect() {
    CloseConnection();
}

void TcpConnect::CloseConnection() {
    force_close_.store(true);
    if (sock != -1) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        sock = -1;
    }
}

bool TcpConnect::IsTerminated() const {
    return force_close_.load();
}

void TcpConnect::ForceClose() {
    force_close_.store(true);
    if (sock != -1) {
        close(sock);
        sock = -1;
    }
}

void TcpConnect::EstablishConnection() {
    force_close_.store(false);

    if (sock != -1) {
        close(sock);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    fd_set fdset;
    struct timeval time_val;

    int current_state = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, current_state | O_NONBLOCK);
    int code = connect(sock, (struct sockaddr*) &server, sizeof(struct sockaddr_in));
    if (code == 0) {
        current_state = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, current_state & ~O_NONBLOCK);
        return;
    }

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    time_val.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(connect_timeout).count();
    time_val.tv_usec = 0;

    code = select(sock + 1, NULL, &fdset, NULL, &time_val);
    switch (code) {
        case 0:
            close(sock);
            sock = -1;
            throw std::runtime_error("Connection timeout");
            break;

        case 1:
        default:
            int soError;
            socklen_t ln = sizeof soError;
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &soError, &ln);

            if (soError == 0) {
                current_state = fcntl(sock, F_GETFL, 0);
                fcntl(sock, F_SETFL, current_state & ~O_NONBLOCK);
                return;
            }

            close(sock);
            sock = -1;
            throw std::runtime_error("Socket connection error");
    }
}

void TcpConnect::SendData(const std::string& data) const {
    if (force_close_.load() || sock == -1) {
        throw std::runtime_error("Connection closed");
    }

    char to_send[BUFSIZ];
    for (size_t i = 0; i < data.size(); ++i) {
        to_send[i] = data[i];
    }

    if ((send(sock, to_send, data.size(), 0)) < 0) {
        if (!force_close_.load()) {
            std::cout << "Send error: " << strerror(errno) << ' ' << errno << '\n';
        }
        throw std::runtime_error("Send error");
    }
}

std::string TcpConnect::ReceiveData(size_t buffer_size) const {
    if (force_close_.load() || sock == -1) {
        throw std::runtime_error("Connection closed");
    }

    std::string message;
    if (!buffer_size) {
        struct pollfd fd;
        fd.fd = sock;
        fd.events = POLLIN;
        int cd = poll(&fd, 1, read_timeout.count());

        if (force_close_.load()) {
            throw std::runtime_error("Connection terminated");
        }

        switch (cd) {
            case -1:
                if (!force_close_.load()) {
                    throw std::runtime_error("Poll error");
                }
                throw std::runtime_error("Connection terminated");

            case 0:
                throw std::runtime_error("Read timeout");

            default:
                char data[4];
                int received = recv(sock, data, sizeof(data), 0);
                if (received <= 0) {
                    if (!force_close_.load()) {
                        throw std::runtime_error("Read error");
                    }
                    throw std::runtime_error("Connection terminated");
                }
                for (int i = 0; i < received; ++i) {
                    message += data[i];
                }
        }

        buffer_size = utils::BytesToInt(message);
    }

    if (buffer_size > 100'000) {
        throw std::runtime_error("Too much data");
    }

    int to_read = buffer_size;
    char data_2[buffer_size];
    while (to_read > 0) {
        if (force_close_.load()) {
            throw std::runtime_error("Connection terminated");
        }

        int read = recv(sock, data_2, sizeof(data_2), 0);
        if (read <= 0) {
            if (!force_close_.load()) {
                throw std::runtime_error("Read error");
            }
            throw std::runtime_error("Connection terminated");
        }
        for (int i = 0; i < read; ++i) {
            message += data_2[i];
        }
        to_read -= read;
    }

    return message;
}

const std::string &TcpConnect::GetIp() const {
    return ip;
}

int TcpConnect::GetPort() const {
    return port;
}
