#pragma once

#include "noncopyable.h"

#include "Callbacks.h"
#include "Buffer.h"
#include "InetAddress.h"
#include "Timestamp.h"

#include <memory>
#include <atomic>
#include <cstring>

class Channel;
class EventLoop;
class Socket;

/**
 * logical idea:
 * TcpServer  =>  Accepter  =>  新用户连接，通过accept拿到connfd  =>  set callback to TcpConnection
 * =>  set callback to Channel  =>  register Poller  =>  Listen to the event =>  Channel 回调操作
 */

// Pack a communication link to successfully connect to the server
// The communication link between the connected client and server
class TcpConnection : noncopyable, 
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    // Constructs a TcpConnection with a connected sockfd
    TcpConnection(EventLoop* loop, 
                  const std::string& nameArg,
                  int scokfd, 
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();
    
    // void send(std::string&& message); C++11
    void send(const std::string& data);
    void shutdown();  // close the connection

    void connectEstablished(); // called when TcpServer accepts a new connection (should be called only once)
    void connectDestroyed(); // called when TcpServer has removed me from its map (should be called only once)

    /******************************************************/

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = std::move(cb); }

    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = std::move(cb); }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = std::move(cb); }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = std::move(cb); highWaterMark_ = highWaterMark; }

    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = std::move(cb); }

    Buffer* inputBuffer() { return &inputBuffer_; }
    Buffer* outputBuffer() { return &outputBuffer_; }

    EventLoop* getloop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }

    /*======================no important=======================*/

    const char* stateToString() const;
    void setTcpNoDelay(bool on);
    void startRead();
    void startReadInLoop();
    void stopRead();
    void stopReadInLoop();

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void setState(StateE s) { state_ = s; }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();

    EventLoop* loop_; // 这里绝不是baseloop, TcpConnection都是在subloop里管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 这里和Acceptor类似 Acceptor=> mainloop  |  TcpConnection=> subloop 
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; // new connection
    MessageCallback messageCallback_; // read/write message
    WriteCompleteCallback writeCompleteCallback_; // After the message is sent
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_;

    Buffer inputBuffer_; //接受数据缓冲区
    Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer. 发送数据缓冲区
};

//typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;