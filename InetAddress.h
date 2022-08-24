#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// template <typename To, typename From>
// inline To implicit_cast(From const& f) { return f; }

// const sockaddr* sockaddr_cast(const sockaddr_in* addr)
// {
//     return static_cast<const sockaddr*>(implicit_cast<const void*>(addr));
// }

// const sockaddr* getSockAddr() const { return sockaddr_cast(&addr_); }

class InetAddress
{
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in& addr);

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getSockAddr() const { return &addr_; }
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }
private:
    sockaddr_in addr_;
};