#include "commands.hpp"

#include "db.hpp"
#include "ping.hpp"
#include "ping_session.hpp"
#include "dispatcher.hpp"

#include <fstream>
#include <mutex>
#include <unordered_map>
#include <thread>

using namespace std;
using namespace TgBot;

unordered_map<long long, PingSession> sessions;
mutex sessionMutex;

void generateGraphForIp(const string& ip) {
    string command = "python draw_graph.py " + ip;
    system(command.c_str());
}

void handleAddIp(Bot& bot, Message::Ptr message) {
    long long chatId = message->chat->id;
    string ip = message->text.substr(7);
    if (ip.empty()) {
        bot.getApi().sendMessage(chatId, u8"Ошибка: укажите IP-адрес после команды /add_ip");
        return;
    }

    if (addMonitoredIp(chatId, ip)) {
        bot.getApi().sendMessage(chatId, u8"IP-адрес " + ip + " добавлен в список мониторинга.");
    }
    else {
        bot.getApi().sendMessage(chatId, u8"IP-адрес уже есть в списке.");
    }
}

void handleListIp(Bot& bot, Message::Ptr message) {
    long long chatId = message->chat->id;
    vector<string> ips = getMonitoredIps(chatId);

    if (ips.empty()) {
        bot.getApi().sendMessage(chatId, u8"Список IP-адресов для мониторинга пуст.");
        return;
    }

    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);
    for (const auto& ip : ips) {
        InlineKeyboardButton::Ptr btn(new InlineKeyboardButton);
        btn->text = ip;
        btn->callbackData = "ping_" + ip;
        keyboard->inlineKeyboard.push_back({ btn });
    }

    bot.getApi().sendMessage(chatId, u8"Выберите IP-адрес для пинга:", false, 0, keyboard);
}

void startPing(Bot& bot, Message::Ptr message) {
    string ip = message->text.substr(6);
    if (ip.empty()) {
        bot.getApi().sendMessage(message->chat->id, u8"Ошибка: введите IP-адрес после команды /ping");
        return;
    }

    logFirstConnection(message->chat->id);

    lock_guard<mutex> lock(sessionMutex);
    if (sessions.count(message->chat->id)) {
        bot.getApi().sendMessage(message->chat->id, u8"Мониторинг уже запущен.");
        return;
    }

    auto& session = sessions[message->chat->id];
    session.ip = ip;
    session.pingThread = thread(pingFunction, ref(bot), message, ip, ref(session.stopFlag));

    bot.getApi().sendMessage(message->chat->id, u8"Мониторинг запущен по адресу: " + ip);
}

void stopPing(Bot& bot, Message::Ptr message) {
    lock_guard<mutex> lock(sessionMutex);
    auto it = sessions.find(message->chat->id);
    if (it == sessions.end()) {
        bot.getApi().sendMessage(message->chat->id, u8"Мониторинг не запущен.");
        return;
    }

    it->second.stopFlag = true;
    if (it->second.pingThread.joinable()) {
        it->second.pingThread.join();
    }
    sessions.erase(it);

    bot.getApi().sendMessage(message->chat->id, u8"Мониторинг остановлен.");
}

void handleReport(Bot& bot, Message::Ptr message) {
    long long chatId = message->chat->id;
    string ip = message->text.substr(8);
    if (ip.empty()) {
        bot.getApi().sendMessage(chatId, u8"Ошибка: введите IP-адрес после команды /report");
        return;
    }

    string csvFile = "report_" + ip + ".csv";
    string pngFile = "graph_" + ip + ".png";

    getDataFromDatabaseByIp(csvFile, ip);
    generateGraphForIp(ip);

    ifstream f1(csvFile), f2(pngFile);
    if (!f1 || f1.peek() == EOF || !f2 || f2.peek() == EOF) {
        bot.getApi().sendMessage(chatId, u8"Ошибка: отчет или график не созданы.");
        return;
    }

    bot.getApi().sendPhoto(chatId, InputFile::fromFile(pngFile, "image/png"),
        u8"График доступности IP Адреса: " + ip);
    bot.getApi().sendDocument(chatId, InputFile::fromFile(csvFile, "text/csv"),
        u8"Отчет по IP Адресу: " + ip);
}

void handleCallbackQuery(Bot& bot, CallbackQuery::Ptr query) {
    if (query->data.find("ping_") == 0) {
        string ip = query->data.substr(6);
        long long chatId = query->from->id;

        if (sessions.count(chatId)) {
            bot.getApi().sendMessage(chatId, u8"Мониторинг уже запущен.");
            return;
        }Message::Ptr fakeMsg(new Message(*query->message));
        fakeMsg->text = "/ping " + ip;
        startPing(bot, fakeMsg);
    }
}

