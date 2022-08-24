#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>

#include <string>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop* loop,
            const InetAddress& addr,
            const std::string& name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
        );

        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, 
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        );
        // 设置合适的loop线程数量 loopthread
        server_.setThreadNum(3);
    }

    void start()
    {
        server_.start();
    }

private:
    // 连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            LOG_INFO("conn UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("conn DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr& conn,
                    Buffer* buf,
                    Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown(); // 写端 EPOLLHUP =>  closeCallback_
    }

    EventLoop* loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "EchoServer-01"); // Acceptor non-blocking listenfd create bind
    server.start(); // 1. 开启底层线程池并运行loop.loop() 2. listen  loopthread  listenfd => acceptChannel => mainLoop
    loop.loop(); // 启动baseloop的底层Poller

    return 0;
}

/**
 * mainloop 和 subloop 之间并没有使用同步队列， 并没有使用生产者消费者模型（异步）
 * 而是使用系统调用eventfd，为每个loop创建了一个weakfd！使用weakfd来做线程间的通知notify
 * 在libevent上使用的socketpair，它是一种双向通信的套接字！
 */