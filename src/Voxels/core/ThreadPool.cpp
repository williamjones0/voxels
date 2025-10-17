#include "ThreadPool.hpp"

void ThreadPool::start() {
    const unsigned int numThreads = std::thread::hardware_concurrency() - 1;
    for (unsigned int i = 0; i < numThreads; i++) {
        threads.emplace_back(&ThreadPool::threadLoop, this);
    }
}

void ThreadPool::threadLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [this] { return !tasks.empty() || shouldTerminate; });
            if (shouldTerminate) {
                break;
            }
            task = tasks.front();
            tasks.pop();
            ++activeTasks;
        }

        task();

        {
            std::unique_lock lock(mutex);
            --activeTasks;
            if (tasks.empty() && activeTasks == 0) {
                doneCv.notify_one();
            }
        }
    }
}

void ThreadPool::queueTask(const std::function<void()>& task) {
    std::unique_lock lock(mutex);
    tasks.push(task);
    cv.notify_one();
}

bool ThreadPool::busy() {
    std::unique_lock lock(mutex);
    return !(tasks.empty() && activeTasks == 0);
}

void ThreadPool::waitUntilDone() {
    std::unique_lock lock(mutex);
    doneCv.wait(lock, [this] { return tasks.empty() && activeTasks == 0; });
}

void ThreadPool::stop() {
    {
        std::unique_lock lock(mutex);
        shouldTerminate = true;
    }
    cv.notify_all();
    for (std::thread& thread : threads) {
        thread.join();
    }
    threads.clear();
}
