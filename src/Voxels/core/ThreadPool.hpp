#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

class ThreadPool {
public:
    void start();
    void queueTask(const std::function<void()>& task);
    bool busy();
    void waitUntilDone();
    void stop();

private:
    void threadLoop();

    bool shouldTerminate = false;
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;

    int activeTasks = 0;
    std::condition_variable doneCv;
};
