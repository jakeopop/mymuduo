#include "TcpConnection.h"
#include "Logger.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"

#include <errno.h>
#include <functional>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cassert>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnectionLoop is null! \n", __FILE__,__FUNCTION__, __LINE__);
    }
    return loop;
}

/*-------------------------------------------------------------*/

TcpConnection::TcpConnection(EventLoop* loop, 
                  const std::string& nameArg,
                  int sockfd, 
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr)
    :   loop_(CheckLoopNotNull(loop)),
        state_(kConnecting),
        name_(nameArg),
        reading_(true),
        socket_(new Socket(sockfd)),
        channel_(new Channel(loop, sockfd)),
        localAddr_(localAddr),
        peerAddr_(peerAddr),
        highWaterMark_(64*1024*1024)  // 64M
{
    // 下面给出channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会毁掉相应的操作函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));
    LOG_INFO("TcpConnection::ctor[%s] at fd=%d \n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(),  (int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    loop_->isInLoopThread();
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if (n > 0)
    {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);   
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) // send completed
            {
                channel_->disableWriting(); // not writable
                if (writeCompleteCallback_)
                {
                    // 唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleErite");

        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}


/*--------------------------------------------------------------------------------*/
// poller => channel::closeCallback => TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected); // 将状态设置成disconnected
    channel_->disableAll();

    // 获取当前对象的智能指针
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); //执行连接关闭的回调 --> 直接再次调用testserver::onConnection回调方法
    closeCallback_(connPtr); // 关闭连接的回调, 执行TcpServer::removeConnection回调方法【TcpServer::123】
}
/*--------------------------------------------------------------------------------*/


void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}


// efficiency!!! 数据 => json / pb
void TcpConnection::send(const std::string& buf)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
            // buf->retrieveAll();
        }
        else
        {
            // fp 指向 sendInLoop
            // void (TcpConnection::*fp)(const void* message, size_t len) = &TcpConnection::sendInLoop;
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));
        }
    }
}


/**
 * 发送数据 应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调
 */
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    ssize_t remaining = len;
    bool faultError = false;

    // 之前调用过该connection的shutdown，不能再进行发送
    if (state_ == kDisconnecting)
    {
        LOG_INFO("disconnected, give up writing!");
        return;
    }

    // if no thing in output queue, try weiting directly
    // 表示channel_第一次开始写数据，而且缓冲区没有待发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_)
            {
                // 既然这里数据全部发送完成，就不用再给channel设置epollout事件
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_INFO("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) 
                {
                    faultError = true;
                }
            }
        }
    }

    /**
     *  说明当前这一次write，并没有把数据全部发送出去，剩余的数据需保存再缓冲区当中
     *  然后给channel注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel
     *  调用eriteCallback_回调，也就是调用TcpConnection::handleWrite，把发送缓冲区中的数据全部发送完成
     */
    if (!faultError && remaining > 0)
    {
        // 目前发送缓冲区剩余的待发送数据的长度
        size_t oldlen = outputBuffer_.readableBytes();

        if (oldlen + remaining >= highWaterMark_
            && oldlen < highWaterMark_
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining));
        }

        outputBuffer_.append((char*)data + nwrote, remaining);
        if (!channel_->isWriting())
        {
            channel_->enableWriting(); // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout
        }
    }
}


// 连接建立
void TcpConnection::connectEstablished()
{
    assert(state_ == kConnecting);

    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 向poller注册channel的epollin事件

    // 新连接建立，执行回调
    connectionCallback_(shared_from_this());   
}


void TcpConnection::connectDestroyed() // 这个if语句一般情况下是进不来的，因为处理TcpConnection::handleClose()：119时已经调用过一次
{
    if (state_ == kConnected) 
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel的所有感兴趣的事件，从poller中del

        connectionCallback_(shared_from_this());
    }
    channel_->remove(); // 把channel从poller中删除 【大概只有这句是能运行的】！！！
}


//关闭连接
void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this)
        );
    }
} 
void TcpConnection::shutdownInLoop()
{
    if (!channel_->isWriting()) // 说明outputBuffer中的数据已经全部发送完成
    {
        socket_->shutdownWrite(); // 关闭写端
    }
}


/*==========================no important===============================*/

const char* TcpConnection::stateToString() const
{
    switch (state_)
    {
        case kDisconnected:
            return "kDisconnected";
        case kConnecting:
                return "kConnecting";
        case kConnected:
                return "kConnected";
        case kDisconnecting:
                return "kDisconnecting";
        default:
                return "unknown state";
    }
}


void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}


void TcpConnection::startRead()
{
    loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
}
void TcpConnection::startReadInLoop()
{
    if (!reading_ || !channel_->isReading())
    {
        channel_->enableReading();
        reading_ = true;
    }
}

void TcpConnection::stopRead()
{
    loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
}
void TcpConnection::stopReadInLoop()
{
    if (reading_ || channel_->isReading())
    {
        channel_->disableReading();
        reading_ = false;
    }
}
