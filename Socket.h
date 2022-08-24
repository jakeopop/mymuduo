#pragma once

#include "noncopyable.h"

class InetAddress;

// 封装socket fd
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {}

    ~Socket();

    int fd() const { return sockfd_; }

    void bindAddress(const InetAddress& Localaddr);
    void listen();

    /**
     *  On success, returns a non-negative integer that is
     *  a descriptor for the accepted socket, which has been
     *  set to non-blocking and close-on-exec. *peeraddr is assigned.
     *  On error, -1 is returned, and *peeraddr is untouched.
     */
    int accept(InetAddress* perraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on); // Enable/disable TCP_NODELAY
    void setReuseAddr(bool on); // Enable/disable SO_REUSEADDR
    void setReusePort(bool on); // Enable/disable SO_REUSEPORT
    void setKeepAlive(bool on); // Enable/disable SO_KEEPALIVE

private:
    const int sockfd_;
};