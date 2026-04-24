//
// Created by Loannth5 on 26/5/25.
//

#ifndef VIDEOEDITOR_SINGLE_THREAD_EXECUTOR_H
#define VIDEOEDITOR_SINGLE_THREAD_EXECUTOR_H

#include <unistd.h>
#include <thread>
#include <queue>

class SingleThreadExecutor {
public:
    const char *tag;

    SingleThreadExecutor(const char *t) : tag(t), stop(false) {
        worker = std::thread([this] {
            this->run();
        });
    }

    ~SingleThreadExecutor() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            stop = true;
        }
        cond.notify_one();
        worker.join();
    }

    // Run a function in the existing thread
    void runInThread(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            tasks.push(std::move(task));
        }
        cond.notify_one();
    }

private:
    std::thread worker;
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable cond;
    bool stop;

    void run() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cond.wait(lock, [this] { return stop || !tasks.empty(); });

                if (stop && tasks.empty())
                    return;

                task = std::move(tasks.front());
                tasks.pop();
            }
            task(); // Execute task on this thread
        }

    }
};

#endif //VIDEOEDITOR_SINGLE_THREAD_EXECUTOR_H