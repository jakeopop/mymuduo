#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channel;
class Poller;


// Reactor, at most one per thread.
// 事件循环类 主要包含两个大模块 Channel Poller(epoll的抽象 )
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // Must be called in the same thread as creation of the object.
    void loop(); // 开启事件循环

    void quit(); // 退出事件循环

    // Time when poll returns, usually means data arrival.
    Timestamp pollReturnTime() const { return pollReturnTime_; } 

    void runInLoop(Functor cb); // 在当前loop中执行cb
    void queueInLoop(Functor cb); // 把cb放入队列中，唤醒loop所在的线程，执行cb

    void wakeup(); // 用来唤醒loop所在的线程

    // EventLoop的方法 =》 Poller的方法
    void updateChannel(Channel* channel); 
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
    void handleRead();  // waked up
    void doPendingFunctors(); // 执行回调

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_; // 原子操作，通过CAS实现
    std::atomic_bool quit_; //标志退出loop循环

    const pid_t threadId_; // 记录当前loop所在线程id

    Timestamp pollReturnTime_; // poller返回发生事件的channel的时间点
    std::unique_ptr<Poller> poller_;

    /**
     * 主要作用:
     * 当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
     * 核心是使用eventfd(线程间通信)  |  libevent核心使用socketpair(网络通信:子loop和主loop之间创建双向通信socket)
     */
    int wakeupFd_; 
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; // 存储loop需要执行的所有回调操作
    std::mutex mutex_; // 互斥锁，保护上面vector容器的线程安全操作
};