#include "net/TcpConnection.hpp"

#include <cstdio>

#include "utils/byte_tools.hpp"

using namespace std::chrono_literals;

TcpConnection::TcpConnection(std::string ip, int port,
                             std::chrono::milliseconds connect_timeout,
                             std::chrono::milliseconds read_timeout) :
      ip(std::move(ip)),
      port(port),
      connect_timeout(connect_timeout),
      read_timeout(read_timeout),
      socket_fd(-1)
{}

TcpConnection::~TcpConnection() {
    CloseConnection();
}

void TcpConnection::CloseConnection() {
    force_close.store(true);
    if (socket_fd != -1) {
        shutdown(socket_fd, SHUT_RDWR);
        close(socket_fd);
        socket_fd = -1;
    }
}

bool TcpConnection::IsTerminated() const {
    return force_close.load();
}

void TcpConnection::ForceClose() {
    force_close.store(true);
    if (socket_fd != -1) {
        close(socket_fd);
        socket_fd = -1;
    }
}

void TcpConnection::EstablishConnection() {
    force_close.store(false);

    if (socket_fd != -1) {
        close(socket_fd);
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip.c_str());
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    fd_set fdset;
    struct timeval time_val;

    int current_state = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, current_state | O_NONBLOCK);
    int code = connect(socket_fd, (struct sockaddr*) &server, sizeof(struct sockaddr_in));
    if (code == 0) {
        current_state = fcntl(socket_fd, F_GETFL, 0);
        fcntl(socket_fd, F_SETFL, current_state & ~O_NONBLOCK);
        return;
    }

    FD_ZERO(&fdset);
    FD_SET(socket_fd, &fdset);
    time_val.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(connect_timeout).count();
    time_val.tv_usec = 0;

    code = select(socket_fd + 1, nullptr, &fdset, nullptr, &time_val);
    switch (code) {
        case 0:
            close(socket_fd);
            socket_fd = -1;
            throw std::runtime_error("Connection timeout");
            break;

        case 1:
        default: {
            int so_error;
            socklen_t len = sizeof(so_error);
            getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &so_error, &len);

            if (so_error == 0) {
                current_state = fcntl(socket_fd, F_GETFL, 0);
                fcntl(socket_fd, F_SETFL, current_state & ~O_NONBLOCK);
                return;
            }

            close(socket_fd);
            socket_fd = -1;
            throw std::runtime_error("Socket connection error");
            break;
        }
    }
}

void TcpConnection::SendData(const std::string& data) const {
    if (force_close.load() || socket_fd == -1) {
        throw std::runtime_error("Connection closed");
    }

    if ((send(socket_fd, data.data(), data.size(), 0)) < 0) {
        throw std::runtime_error("Send error");
    }
}

std::string TcpConnection::ReceiveData(size_t buffer_size) const {
    if (force_close.load() || socket_fd == -1) {
        throw std::runtime_error("Connection closed");
    }

    std::string message;
    if (buffer_size == 0) {
        struct pollfd fd;
        fd.fd = socket_fd;
        fd.events = POLLIN;
        int code = poll(&fd, 1, read_timeout.count());

        if (force_close.load()) {
            throw std::runtime_error("Connection terminated");
        }

        switch (code) {
            case -1:
                if (!force_close.load()) {
                    throw std::runtime_error("Poll error");
                }
                throw std::runtime_error("Connection terminated");
                break;

            case 0:
                throw std::runtime_error("Read timeout");
                break;

            default: {
                char data[4];
                int received = recv(socket_fd, data, sizeof(data), 0);
                if (received <= 0) {
                    if (!force_close.load()) {
                        throw std::runtime_error("Read error");
                    }
                    throw std::runtime_error("Connection terminated");
                }
                message.append(data, received);
                break;
            }
        }

        buffer_size = utils::BytesToInt32(message);
    }

    if (buffer_size > 100000) {
        throw std::runtime_error("Too much data");
    }

    int to_read = buffer_size;
    while (to_read > 0) {
        if (force_close.load()) {
            throw std::runtime_error("Connection terminated");
        }

        constexpr size_t kBufferSize = 4096;
        char buffer[kBufferSize];
        int read_size = std::min(static_cast<int>(kBufferSize), to_read);
        
        int received = recv(socket_fd, buffer, read_size, 0);
        if (received <= 0) {
            if (!force_close.load()) {
                throw std::runtime_error("Read error");
            }
            throw std::runtime_error("Connection terminated");
        }
        
        message.append(buffer, received);
        to_read -= received;
    }

    return message;
}

const std::string& TcpConnection::GetIp() const {
    return ip;
}

int TcpConnection::GetPort() const {
    return port;
}
