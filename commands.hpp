#pragma once

#include <tgbot/tgbot.h>

void handleAddIp(TgBot::Bot& bot, TgBot::Message::Ptr message);
void handleListIp(TgBot::Bot& bot, TgBot::Message::Ptr message);
void startPing(TgBot::Bot& bot, TgBot::Message::Ptr message);
void stopPing(TgBot::Bot& bot, TgBot::Message::Ptr message);
void handleReport(TgBot::Bot& bot, TgBot::Message::Ptr message);
void handleCallbackQuery(TgBot::Bot& bot, TgBot::CallbackQuery::Ptr query);