#pragma once
#include <string>
#include <atomic>
#include <tgbot/tgbot.h>

bool pingDevice(const std::string& ip);
void pingFunction(TgBot::Bot& bot, TgBot::Message::Ptr message, std::string ip, std::atomic<bool>& stopFlag);