#include "net/UdpClient.hpp"

#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <random>

UdpClient::UdpClient(const std::string& host, int port, int timeout_sec)
    : host(host), port(port), timeout_sec(timeout_sec)
{

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error(
            std::string("[UdpClient] Failed to create socket: ") + strerror(errno));
    }

    struct timeval tv{};
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        close(sockfd);
        throw std::runtime_error(
            std::string("[UdpClient] Failed to set SO_RCVTIMEO: ") + strerror(errno));
    }

    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        close(sockfd);
        throw std::runtime_error(
            "[UdpClient] Failed to resolve host " + host + ": " +
            std::string(hstrerror(h_errno)));
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    memcpy(&server_address.sin_addr.s_addr, server->h_addr, server->h_length);

}

UdpClient::~UdpClient() {
    if (sockfd >= 0) {
        close(sockfd);
    }
}

std::string UdpClient::SendReceive(const std::string& data) {

    ssize_t sent = sendto(sockfd, data.data(), data.size(), 0,
                          reinterpret_cast<sockaddr*>(&server_address),
                          sizeof(server_address));

    if (sent < 0) {
        throw std::runtime_error(
            "[UdpClient] Send error to " + host + ":" +
            std::to_string(port) + ": " + strerror(errno));
    }

    if (sent != (ssize_t)data.size()) {
        throw std::runtime_error(
            "[UdpClient] Partial send: " + std::to_string(sent) +
            " of " + std::to_string(data.size()) + " bytes");
    }

    std::vector<char> buffer(BUFSIZ);
    socklen_t address_length = sizeof(server_address);

    ssize_t received = recvfrom(sockfd, buffer.data(), buffer.size(), 0,
                                reinterpret_cast<sockaddr*>(&server_address),
                                &address_length);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            throw std::runtime_error(
                "[UdpClient] Timeout waiting for response (" +
                std::to_string(timeout_sec) + "s) from " +
                host + ":" + std::to_string(port));
        }
        throw std::runtime_error(
            "[UdpClient] Receive error from " + host + ":" +
            std::to_string(port) + ": " + strerror(errno));
    }


    return std::string(buffer.data(), received);
}

uint64_t UdpClient::GenerateTransactionId() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    static thread_local std::uniform_int_distribution<uint64_t> dist;
    return dist(rng);
}
