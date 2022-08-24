#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking()
{
    int listenfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return listenfd;
}

/*--------------------------------------------------------------------------------------------------*/

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking()) // create listen-socket
    , acceptChannel_(loop_, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr); // bind
    /*====================================================================*/
    /**
     * !绑定回调! [它只关心读事件]
     * TcpServer::start() Acceptor.listen 有新用户连接，要执行一个回调(connfd=>channel=>subloop)
     * baseLoop => acceptChannel_(listenfd)【收到有客户端连接】 => 执行channel的回调函数【这里将handleRead设置为回调函数】
     */
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
    /*====================================================================*/
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

/*===========================*/
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listen
    acceptChannel_.enableReading(); // 将acceptChannel_[listenfd的包装]注册 => Poller
}
/*===========================*/

/*============================================*/
// listenfd有事件发生了，有新用户连接
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr); // 轮询找到subloop，唤醒，分发当前的channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) // The per-process limit on the number of open file descrptors has been reached. 解决方法：集群/分布式部署
        {
            LOG_ERROR("%s:%s:%d socket reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
/*============================================*/