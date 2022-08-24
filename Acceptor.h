#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

class EventLoop;
class InetAddress;

/**
 * Acceptor of incoming TCP connections.
 */
class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) 
        { newConnectionCallback_ = std::move(cb); }

    bool listenning() const { return listenning_; }
    void listen();
private:
    void handleRead();

    EventLoop* loop_; // Accptor用的就是用户定义的那个baseloop，也成为mainLoop
    Socket acceptSocket_;
    Channel acceptChannel_; // clientfd conn success!!!
    NewConnectionCallback newConnectionCallback_; // 将acceptSocket打包成channel、channel传递给subloop
    bool listenning_;
};