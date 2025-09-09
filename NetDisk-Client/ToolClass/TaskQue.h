#ifndef TASKQUE_H
#define TASKQUE_H

#include "DisallowCopyAndMove.h"
#include <QObject>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>

class TaskQue {
public:
    // 禁止拷贝和移动
    DISALLOW_COPY_AND_MOVE(TaskQue);

    TaskQue() = default;
    explicit TaskQue(size_t thread_count = 1);
    ~TaskQue();

    void close();   // 关闭队列
    template<class F>
    void addTask(F&& task);

private:
    void doing();   // 工作线程处理函数

private:
    std::queue<std::function<void()>> task_queue_;  // 任务队列
    std::vector<std::thread> threads_;              // 线程数组
    std::condition_variable cv_;
    std::mutex mtx_;
    bool is_close_{ false };
};

// 在线程池中添加任务
template <class F>
void TaskQue::addTask(F &&task) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (is_close_) {    // 线程池已经关闭
            throw std::runtime_error("ThreadPool already stop, can't add task");
        }
        task_queue_.emplace(std::forward<F>(task));
    }
    cv_.notify_one();  // 唤醒一个线程执行该任务
}

#endif // TASKQUE_H
