#pragma once 

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// TCP server, supports single-threaded and thread-pool models.
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop* loop,
                const InetAddress& listenAddr,
                const std::string& nameArg,
                Option option = kNoReusePort);
    ~TcpServer();

    const std::string& ipPort() { return ipPort_; }
    const std::string& name() { return name_; }
    EventLoop* getLoop() { return loop_; }

    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = std::move(cb); }
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = std::move(cb); }

    // set底层SubLoop的个数
    void setThreadNum(int numThreads);

    // 开启Server监听
    void start();

private:
    void newConnection(int sockfd, const InetAddress& peerAddr); // 给 Acceptor::handleRead 传递的[对新连接对象处理]的回调函数! 
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;


    // 重点
    EventLoop* loop_; // baseloop 用户定义的loop [the acceptor loop]
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_; // run in mainLoop，任务是监听新连接事件
    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

    ConnectionCallback connectionCallback_; // 有新连接的回调
    MessageCallback messageCallback_; // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成之后的回调

    ThreadInitCallback threadInitCallback_; // loop线程初始化的回调
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_; // save all connections
};