#include "TaskQue.h"

TaskQue::TaskQue(size_t thread_count)
    : is_close_(false)
{
    // 创建线程
    for (size_t i=0; i<thread_count; ++i) {
        threads_.emplace_back(&TaskQue::doing, this);
    }
}

TaskQue::~TaskQue() {
    // 关闭线程池
    close();
}

void TaskQue::close() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (is_close_ == true) {    // 已经关闭
            return;
        }
        is_close_ = true;   // 关闭
    }

    cv_.notify_all();   // 通知所有线程
    // 清除线程
    for (std::thread &th : threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
}

void TaskQue::doing() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]() { return is_close_ || !task_queue_.empty(); });

            if (is_close_ && task_queue_.empty()) {
                return;  // 队列关闭且无任务时退出
            }

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }
        if (task) {
            task();  // 执行任务
        }
    }
}
