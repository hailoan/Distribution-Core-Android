//
// Single render-thread executor for the videolib OpenGL ES preview.
//
// Every EGL/GL call must run on exactly one thread with the context current
// (see opengles-guideline). This primitive owns that thread and serializes work
// posted to it. Adapted from the donor videogl SingleThreadExecutor, with an
// added synchronous variant so a JNI caller can observe completion (init result,
// zero-copy frame upload lifetime, ordered teardown).
//

#ifndef VIDEOLIB_RENDER_THREAD_EXECUTOR_H
#define VIDEOLIB_RENDER_THREAD_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

class RenderThreadExecutor {
public:
    explicit RenderThreadExecutor(const char *tag) : tag_(tag), stop_(false) {
        worker_ = std::thread([this] { run(); });
    }

    ~RenderThreadExecutor() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cond_.notify_one();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    RenderThreadExecutor(const RenderThreadExecutor &) = delete;
    RenderThreadExecutor &operator=(const RenderThreadExecutor &) = delete;

    // Fire-and-forget: runs the task on the render thread; returns immediately.
    void runAsync(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cond_.notify_one();
    }

    // Blocks the caller until the task has finished running on the render thread.
    // `task` and the promise outlive execution because we wait on the future.
    void runSync(const std::function<void()> &task) {
        std::promise<void> done;
        std::future<void> fut = done.get_future();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.push([&task, &done] {
                task();
                done.set_value();
            });
        }
        cond_.notify_one();
        fut.wait();
    }

private:
    const char *tag_;
    std::thread worker_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stop_;

    void run() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }
};

#endif // VIDEOLIB_RENDER_THREAD_EXECUTOR_H
