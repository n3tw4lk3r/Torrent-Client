#pragma once

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstdint>
#include <string>

class UdpConnection {
public:
    UdpConnection(const std::string& host, int port, int timeout_sec = 10);
    ~UdpConnection();

    std::string SendReceive(const std::string& data);
    uint64_t GenerateTransactionId();

private:
    int socket_fd;
    struct sockaddr_in server_address;
    std::string host;
    int port;
    int timeout_sec;
};
