#include "dispatcher.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>

static std::queue<std::function<void()>> eventQueue;
static std::mutex queueMutex;
static std::condition_variable queueCondVar;

void pushEvent(std::function<void()> func) {
    std::lock_guard<std::mutex> lock(queueMutex);
    eventQueue.push(std::move(func));
    queueCondVar.notify_one();
}

void eventDispatcher() {
    while (true) {
        std::cout << "work";
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondVar.wait(lock, [] { return !eventQueue.empty(); });
            task = std::move(eventQueue.front());
            eventQueue.pop();
        }
        try {
            std::cout << "work2";
            task();
        }
        catch (const std::exception& e) {
            std::cerr << "Ошибка при обработке события: " << e.what() << std::endl;
        }
    }
    std::cout << "work3";
}