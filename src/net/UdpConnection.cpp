#include "net/UdpConnection.hpp"

#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>

UdpConnection::UdpConnection(const std::string& host, int port, int timeout_sec) :
    host(host),
    port(port),
    timeout_sec(timeout_sec)
{
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error(
            std::string("[UdpConnection] Failed to create socket: ") + strerror(errno));
    }

    struct timeval tv{};
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        close(socket_fd);
        throw std::runtime_error(
            std::string("[UdpConnection] Failed to set SO_RCVTIMEO: ") + strerror(errno));
    }

    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        close(socket_fd);
        throw std::runtime_error(
            "[UdpConnection] Failed to resolve host " + host + ": " +
            std::string(hstrerror(h_errno)));
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    memcpy(&server_address.sin_addr.s_addr, server->h_addr, server->h_length);
}

UdpConnection::~UdpConnection() {
    if (socket_fd >= 0) {
        close(socket_fd);
    }
}

std::string UdpConnection::SendReceive(const std::string& data) {
    ssize_t sent = sendto(socket_fd, data.data(), data.size(), 0,
                          reinterpret_cast<sockaddr*>(&server_address),
                          sizeof(server_address));

    if (sent < 0) {
        throw std::runtime_error(
            "[UdpConnection] Send error to " + host + ":" +
            std::to_string(port) + ": " + strerror(errno));
    }

    if (sent != static_cast<ssize_t>(data.size())) {
        throw std::runtime_error(
            "[UdpConnection] Partial send: " + std::to_string(sent) +
            " of " + std::to_string(data.size()) + " bytes");
    }

    constexpr size_t kBufferSize = 65536;
    std::vector<char> buffer(kBufferSize);
    socklen_t address_length = sizeof(server_address);

    ssize_t received = recvfrom(socket_fd, buffer.data(), buffer.size(), 0,
                                reinterpret_cast<sockaddr*>(&server_address),
                                &address_length);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            throw std::runtime_error(
                "[UdpConnection] Timeout waiting for response (" +
                std::to_string(timeout_sec) + "s) from " +
                host + ":" + std::to_string(port));
        }
        throw std::runtime_error(
            "[UdpConnection] Receive error from " + host + ":" +
            std::to_string(port) + ": " + strerror(errno));
    }

    return std::string(buffer.data(), received);
}

uint64_t UdpConnection::GenerateTransactionId() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    static thread_local std::uniform_int_distribution<uint64_t> dist;
    return dist(rng);
}
