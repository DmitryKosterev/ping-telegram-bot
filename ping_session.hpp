#ifndef PING_SESSION_HPP
#define PING_SESSION_HPP

#include <atomic>
#include <string>
#include <tgbot/tgbot.h>

using namespace std;
using namespace TgBot;

struct PingSession {
    thread pingThread;
    atomic<bool> stopFlag{ false };
    string ip;

    PingSession() = default;

    PingSession(PingSession&& other) noexcept
        : pingThread(std::move(other.pingThread)),
        stopFlag(other.stopFlag.load()),
        ip(std::move(other.ip)) {
    }

    PingSession& operator=(PingSession&& other) noexcept {
        if (this != &other) {
            if (pingThread.joinable()) {
                pingThread.join();
            }
            pingThread = std::move(other.pingThread);
            stopFlag = other.stopFlag.load();
            ip = std::move(other.ip);
        }
        return *this;
    }

    PingSession(const PingSession&) = delete;
    PingSession& operator=(const PingSession&) = delete;
};
#endif