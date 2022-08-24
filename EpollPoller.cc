#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"
#include "Types.h"

#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

// channel的成员index_ = -1 
const int kNew = -1;    // channel未添加到poller中
const int kAdded = 1;   // channel已添加到poller中
const int kDeleted = 2; // channel从poller中删除

EPollPoller::EPollPoller(EventLoop* loop) : 
    Poller(loop),
    epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
    events_(kInitEventListSize)  // vector<epoll_event>
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

/**
 * 重写基类Poller的抽象方法 epoll_wait
 * 还是通过EventLoop => 调用poller.poll方法 
 * poll作用：通过epoll_wait接收到发生事件的channel通过activeChannels填到EventLoop的 ChannelList 中
 */
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func[%s] => fd total count: %lu \n", __FUNCTION__, implicit_cast<size_t>(channels_.size()));

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveError = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (implicit_cast<size_t>(numEvents) == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else
    {
        if (saveError != EINTR)
        {
            errno = saveError;
            LOG_ERROR("EpollPoller::poll() err!");
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller
/**
 *              EventLoop
 *      ChannelList     Poller
 *     (多个channel)   ChannelMap<fd, channel*>    解释：[ChannelList.size() >= ChannelMap.size()]
 */
void EPollPoller::updateChannel(Channel* channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d | events=%d | index =%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)
    {
        // a new one, add with EPOLL_CTL_ADD
        int fd = channel->fd();
        if (index == kNew)
        {
            assert(channels_.find(fd) == channels_.end());
            channels_[fd] = channel;
        }
        else // index == kDeleted
        {
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else  // channel已经在poller上注册过了
    {
        // update existing one with EPOLL_CTL_MOD/DEL
        int fd = channel->fd();
        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);

        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从poller中的ChannelMap删除channel
void EPollPoller::removeChannel(Channel* channel)
{
    int fd = channel->fd();
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);

    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());

    channels_.erase(fd);
    
    int index = channel->index();
    assert(index == kAdded || index == kDeleted);
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}


// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    assert(implicit_cast<size_t>(numEvents) <= events_.size());

    for (int i = 0; i < numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr); // events_.data.ptr指向的就是当前channel的内存地址【绑定channel】
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了
    }
}


// 更新channel通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel* channel)
{
    epoll_event ep_event;
    memset(&ep_event, 0, sizeof ep_event);

    int fd = channel->fd();

    ep_event.events = channel->events();
    ep_event.data.fd = fd;
    ep_event.data.ptr = channel;
    
    
    if (::epoll_ctl(epollfd_, operation, fd, &ep_event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d \n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d \n", errno);
        }
    }
}
