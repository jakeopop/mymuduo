#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class Channel;
/**
 * epoll使用OOP
 * epoll_create
 * epoll_ctl add/mod/del
 * epoll_wait
 */

class EPollPoller : public Poller
{
public:
    EPollPoller(EventLoop* loop); // epoll_create
    ~EPollPoller() override; 

    // 重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override; // epoll_wait
    void updateChannel(Channel* channel) override; // epoll_ctl add/mod
    void removeChannel(Channel* Channel) override; // epoll_ctl del

private:
    static const int kInitEventListSize = 16;

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    // 更新channel通道
    void update(int operation, Channel* channel);

    /**
     * @Poller抽象类中继承来了关于channel的map集合
     * typedef std::unordered_map<int, Channel*>;
     * ChannelMap channels_;
     */

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};