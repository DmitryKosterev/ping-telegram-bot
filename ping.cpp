#include "ping.hpp"
#include "ping_session.hpp"
#include "db.hpp"
#include "dispatcher.hpp"

#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using namespace TgBot;

// Пинг устройства
bool pingDevice(const string& ip) {
#ifdef _WIN32
    string command = "ping -n 1 " + ip;
#else
    string command = "ping -c 1 " + ip;
#endif
    return system(command.c_str()) == 0;
}

// Поток пинга
void pingFunction(Bot& bot, Message::Ptr message, string ip, atomic<bool>& stopFlag) {
    long long chatId = message->chat->id;
    int i = 0;

    auto threadId = this_thread::get_id();
    cout << threadId;
    
    while (!stopFlag) {
        try {
            auto start = chrono::system_clock::now();
            bool reachable = pingDevice(ip);
            auto end = chrono::system_clock::now();
            chrono::duration<double> elapsed = end - start;

            if (reachable) {
                if (i > 0) {
                    pushEvent([chatId, &bot]() {
                        bot.getApi().sendMessage(chatId, u8"Соединение восстановлено");
                        });
                    logIncident(chatId, ip, "Connection restored");
                    i = 0;
                }

                if (elapsed > chrono::seconds(5)) {
                    pushEvent([chatId, &bot]() {
                        bot.getApi().sendMessage(chatId, u8"Снизилась скорость передачи пакетов");
                        });
                    logIncident(chatId, ip, "High latency");
                }
            }
            else {
                i++;
                pushEvent([chatId, &bot]() {
                    bot.getApi().sendMessage(chatId, u8"Устройство недоступно!");
                    });
                logIncident(chatId, ip, "Not responding");
            }

            for (int t = 0; t < 20 && !stopFlag; ++t) {
                this_thread::sleep_for(chrono::seconds(1));
            }
        }
        catch (exception& e) {
            cerr << "Ping error: " << e.what() << endl;
        }
    }
}