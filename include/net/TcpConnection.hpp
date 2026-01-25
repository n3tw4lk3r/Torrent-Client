#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>

class TcpConnection {
public:
    explicit TcpConnection(std::string ip, int port, 
                           std::chrono::milliseconds connect_timeout, 
                           std::chrono::milliseconds read_timeout);
    ~TcpConnection();

    void EstablishConnection();
    void SendData(const std::string& data) const;
    std::string ReceiveData(size_t buffer_size = 0) const;
    void CloseConnection();
    void ForceClose();
    const std::string& GetIp() const;
    int GetPort() const;
    bool IsTerminated() const;

private:
    const std::string ip;
    const int port;
    std::chrono::milliseconds connect_timeout;
    std::chrono::milliseconds read_timeout;
    mutable std::atomic<bool> force_close{false};
    mutable int socket_fd;
};
