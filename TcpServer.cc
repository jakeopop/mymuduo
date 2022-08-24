#include "TcpServer.h"
#include "Logger.h"

#include <string.h>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__,__FUNCTION__, __LINE__);
    }
    return loop;
}

struct sockaddr_in getLocalAddr(int sockfd)
{
    struct sockaddr_in localaddr;
    ::bzero(&localaddr, sizeof localaddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
    if (::getsockname(sockfd, (sockaddr*)&localaddr, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    return localaddr;
}

/*====================================================================================*/

TcpServer::TcpServer(EventLoop* loop, 
                const InetAddress& listenAddr,
                const std::string& nameArg,
                Option option)
                : loop_(CheckLoopNotNull(loop))
                , ipPort_(listenAddr.toIpPort())
                , name_(nameArg)
                , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
                , threadPool_(new EventLoopThreadPool(loop, name_)) // 线程池对象创建{未开启线程}，默认main
                , connectionCallback_()
                , messageCallback_()
                , nextConnId_(1)
                , started_(0)
{
    /**
     * 当有新用户连接时，会执行TcpServer::newConnection回调
     * pass two parameter: Acceptor.h -> newConnectionCallback(connfd, peerAddr);
     */
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, 
        std::placeholders::_1, std::placeholders::_2));
}


TcpServer::~TcpServer()
{
    LOG_INFO("TcpServer::~TcpServer [%s] \n", name_.c_str());

    for (auto& item : connections_)
    {
        // 这个局部的share_ptr智能指针对象，出右括号，可以自动释放new出来TcpConnection对象资源了
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        // 销毁连接
        conn->getloop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}


// set底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}


/*-------------------------------------------------------------------*/
// Enable server monitoring 
void TcpServer::start()
{
    if (started_++ == 0) // 防止一个TcpServer对象被start多次
    { 
        threadPool_->start(threadInitCallback_); // 启动底层loop线程池并开启子线程loop.loop()
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get())); // 将accptorChannel注册在mainloop的poller上
    }
}
/*-------------------------------------------------------------------*/


/**
 * 拓展： Acceptor在干什么？
 * Acceptor的构造函数中有一个初始化acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
 *    本质上是为acceptChannel设置读回调函数，当acceptfd接受到信号时会调用此回调函数从而执行Acceptot::handleRead函数
 * 在handleRead函数中存在newConnectionCallback_(connfd, peerAddr)回调函数，那么是谁设置的这个回调函数呢？
 *    答案是：TcpServer中的 acceptor_->setNewConnectionCallback()设置的，它绑定了TcpServer::newConnection函数
 * 所以流程就是：有新用户连接，实际上最终调用执行的还是TcpServer中设置的newConnection函数
 */

// （mainloop处理）有一个新的客户端的来连接，acceptor会执行这个回调【会把客户端的sockfd和Ip端口号传给这个回调】（宛如向上级汇报）
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 轮询算法，选择一个subloop来管理channel
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    InetAddress localAddr(getLocalAddr(sockfd));

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer => TcpConnection => Channel => Poller => notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}


void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn)
    );
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection [%s] \n",
        name_.c_str(), conn->name().c_str());

    size_t n = connections_.erase(conn->name());

    EventLoop* ioLoop = conn->getloop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn)
    );
}