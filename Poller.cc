#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop)
{}

bool Poller::hasChannel(Channel* channel) const
{
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
} 

/**
 * 为什么newDefaultPoller函数不在Poller.cc实现呢？
 * 因为返回值是Poller*类型的，所以内部实现一定和PollPoller和EpollPoller有关
 * 势必会引入#include "PollPoller.h" | #include "PollPoller.h"
 * 基类引用派生类？ 十分不合理！
 */
// #include "PollPoller.h"
// #include "PollPoller.h"
// Poller* Poller::newDefaultPoller(EventLoop* loop)
// {}