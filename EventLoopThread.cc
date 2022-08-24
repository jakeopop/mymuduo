#include "EventLoopThread.h"
#include "EventLoop.h"

/**
 * 解析使用绑定器bind: 因为事实上 EventLoopThread::threadFunc 和 name 这两个参数是传递到 类Thread中的
 *   -> Thread(ThreadFunc, const std::string& name = string());
 * 但是这样传递缺少了本身的信息，故绑定this!
 */
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
            const std::string& name)
    :   loop_(nullptr),
        exiting_(false),
        thread_(std::bind(&EventLoopThread::threadFunc, this), name),   
        mutex_(),
        cond_(),
        callback_(cb)
{}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }    
}

/**
 * @brief 
 * 
 * @return EventLoop* 
 */
EventLoop* EventLoopThread::startLoop()
{
    thread_.start(); // 启动底层新线程【其实就是执行回调threadFunc】

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (loop_ == nullptr)
        {
            cond_.wait(lock); // 等待新线程中已经创建好新loop 线程间通信
        }
        loop = loop_;
    }

    return loop;
}

// 下面这个方法，是在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个独立的eventloop，和上面的线程是一一对应的，one loop per thread
 
    if (callback_) // 如果有需要传递的回调，就可以运行
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop; 
        cond_.notify_one();
    }

    loop.loop(); // EventLoop loop => Poller.poll 一般在这里循环，服务器关闭才会往下执行 

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}