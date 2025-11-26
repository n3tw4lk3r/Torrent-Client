#pragma once

#include <string>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <chrono>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <atomic>

class TcpConnect {
public:
    explicit TcpConnect(std::string ip, int port, std::chrono::milliseconds connectTimeout, std::chrono::milliseconds readTimeout);
    ~TcpConnect();

    void EstablishConnection();
    void SendData(const std::string& data) const;
    std::string ReceiveData(size_t bufferSize = 0) const;
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
    mutable std::atomic<bool> force_close_{false};
    mutable int sock;
};
