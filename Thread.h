#pragma once 

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread : noncopyable
{
public:
    /************************************************************/
    // 要是带参数的类型该咋办？ 绑定器std::bind + 函数对象std::function<> [用途：事件回调]
    /************************************************************/
    using ThreadFunc = std::function<void()>; // 线程函数类型

    explicit Thread(ThreadFunc, const std::string& name = std::string());
    ~Thread();

    void start();
    void join(); 

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_; }
private:
    void setDefaultName();

    bool started_;
    bool joined_;
    // std::thread thread_; 不能这么用，它一经创建便开启线程
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_; // 存储线程函数
    std::string name_;
    static std::atomic_int numCreated_;
};