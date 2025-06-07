#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <vector>
#include <tgbot/tgbot.h>
#include <unordered_set>

#include "db.hpp"
#include "ping.hpp"
#include "ping_session.hpp"
#include "dispatcher.hpp"

using namespace std;
using namespace TgBot;


// Хранилище сессий
unordered_map<long long, PingSession> sessions;
mutex sessionMutex;

unordered_set<long long> awaitingIpInput;
mutex inputMutex;

void generateGraphForIp(long long chatId, const string& ip) {
    string command = "python draw_graph.py " + to_string(chatId) + " " + ip;
    system(command.c_str());
}

void handleStart(Bot& bot, Message::Ptr message) {
    bot.getApi().sendMessage(message->chat->id, "Добро пожаловать в Ping Bot! Обратите внимание, что пока что инструмент находится в разработке");
}

void initiateCommands(Bot& bot, Message::Ptr message) {
    bot.getApi().sendMessage(message->chat->id, "Cписок доступных команд:\n"
        "/help - показать список доступных команд\n"
        "/add_ip ip-address - добавить IP-адрес в список мониторинга\n"
        "/remove_ip ip-address - убрать IP-адрес из списка мониторинга\n"
        "/list_ips - список IP-аресов для мониторинга\n"
        "/ping ip-address — начать мониторинг указанного IP-адреса (он автоматически сохранится в список для мониторинга)\n"
        "/stop — остановить мониторинг\n"
        "/report ip-address — получить отчет и график по указанному IP-адресу");
}

// Команда: /add_ip IP — добавить IP в список мониторинга
void handleAddIp(Bot& bot, Message::Ptr message) {
    long long chatId = message->chat->id;
    string text = message->text;
    if (text.length() <= 8) {
        bot.getApi().sendMessage(chatId, "Ошибка: укажите IP-адрес после команды /add_ip");
        return;
    }
    string ip = text.substr(8);
    if (ip.empty()) {
        bot.getApi().sendMessage(chatId, "Ошибка: укажите IP-адрес после команды /add_ip");
        return;
    }

    if (addMonitoredIp(chatId, ip)) {
        bot.getApi().sendMessage(chatId, "IP-адрес " + ip + " добавлен в список мониторинга.");
    }
    else {
        bot.getApi().sendMessage(chatId, "IP-адрес уже есть в списке.");
    }
}
void sendIpList(Bot& bot, long long chatId) {
    vector<string> ips = getMonitoredIps(chatId);
    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);

    if (ips.empty()) {
        InlineKeyboardButton::Ptr addBtn(new InlineKeyboardButton);
        addBtn->text = "➕ Добавить IP";
        addBtn->callbackData = "add_ip";

        keyboard->inlineKeyboard.push_back({ addBtn });
        bot.getApi().sendMessage(chatId, "📭 Список IP-адресов для мониторинга пуст.", false, 0, keyboard);
        return;
    }

    for (const auto& ip : ips) {
        InlineKeyboardButton::Ptr pingBtn(new InlineKeyboardButton);
        pingBtn->text = "📡 " + ip;
        pingBtn->callbackData = "ping_" + ip;

        InlineKeyboardButton::Ptr reportBtn(new InlineKeyboardButton);
        reportBtn->text = "📊";
        reportBtn->callbackData = "report_" + ip;

        InlineKeyboardButton::Ptr delBtn(new InlineKeyboardButton);
        delBtn->text = "❌";
        delBtn->callbackData = "del_" + ip;

        keyboard->inlineKeyboard.push_back({ pingBtn, reportBtn, delBtn });
    }

    InlineKeyboardButton::Ptr stopBtn(new InlineKeyboardButton);
    stopBtn->text = "🛑 Остановить мониторинг";
    stopBtn->callbackData = "stop_monitoring";

    keyboard->inlineKeyboard.push_back({ stopBtn });

    InlineKeyboardButton::Ptr addBtn(new InlineKeyboardButton);
    addBtn->text = "➕ Добавить IP";
    addBtn->callbackData = "add_ip";

    keyboard->inlineKeyboard.push_back({ addBtn });

    bot.getApi().sendMessage(chatId, "Выберите IP-адрес для пинга, получения отчета или удаления:", false, 0, keyboard);
}
// Команда: /list_ip — показать список IP для мониторинга

void handleListIp(Bot& bot, Message::Ptr message) {
    sendIpList(bot, message->chat->id);
}

void startPing(Bot& bot, Message::Ptr message) {
    string ip;
    long long chatId = message->chat->id;
    try {
        ip = message->text.substr(6);
        if (ip.empty()) {
            bot.getApi().sendMessage(chatId, "Ошибка: введите IP-адрес после команды /ping");
            return;
        }
    }
    catch (...) {
        bot.getApi().sendMessage(chatId, "Ошибка: введите команду в формате /ping IP");
        return;
    }

    lock_guard<mutex> lock(sessionMutex);
    if (sessions.count(chatId)) {
        bot.getApi().sendMessage(chatId, "Мониторинг уже запущен.");
        return;
    }

    addMonitoredIp(chatId, ip);

    // Вставляем в map пустой PingSession с ip
    auto& session = sessions[chatId];
    session.ip = ip;

    // Запускаем поток, передавая ссылку на stopFlag внутри sessions
    session.pingThread = thread(pingFunction, ref(bot), message, ip, ref(session.stopFlag));
    logIncident(chatId, ip, "Connected");
    bot.getApi().sendMessage(chatId, "Мониторинг запущен по адресу: " + ip);
}

// Остановка мониторинга
void stopPing(Bot& bot, Message::Ptr message) {
    lock_guard<mutex> lock(sessionMutex);
    auto it = sessions.find(message->chat->id);
    if (it == sessions.end()) {
        bot.getApi().sendMessage(message->chat->id, "Мониторинг не запущен.");
        return;
    }

    it->second.stopFlag = true;
    if (it->second.pingThread.joinable()) {
        it->second.pingThread.join();
    }
    sessions.erase(it);

    bot.getApi().sendMessage(message->chat->id, "Мониторинг остановлен.");
}

void handleRemove(Bot& bot, Message::Ptr message) {
    long long chatId = message->chat->id;
    string text = message->text;
    
    if (text.length() <= 10) {
        bot.getApi().sendMessage(chatId, "Ошибка: укажите IP-адрес после команды /remove_ip");
        return;
    }

    string ip = text.substr(11);  // после "/remove_ip "

    if (removeMonitoredIp(chatId, ip)) {
        bot.getApi().sendMessage(chatId, "IP-адрес " + ip + " успешно удалён из списка.");
    }
    else {
        bot.getApi().sendMessage(chatId, "IP-адрес не найден в вашем списке.");
    }
}

void handleReport(Bot& bot, Message::Ptr message) {
    long long chatId = message->chat->id;
    string ip;

    try {
        ip = message->text.substr(8);
        if (ip.empty()) {
            bot.getApi().sendMessage(chatId, "Ошибка: введите IP-адрес после команды /report");
            return;
        }
    }
    catch (...) {
        bot.getApi().sendMessage(chatId, "Не удалось сформировать отчет по указанному IP-адресу");
        return;
    }

    string csvFile = "report_" + to_string(chatId) + "_" + ip + ".csv";
    string pngFile = "graph_" + to_string(chatId) + "_" + ip + ".png";

    getDataFromDatabaseByIp(csvFile, chatId, ip);
    generateGraphForIp(chatId, ip);

    ifstream f1(csvFile), f2(pngFile);
    if (!f1 || f1.peek() == EOF || !f2 || f2.peek() == EOF) {
        bot.getApi().sendMessage(chatId, "Ошибка: отчет или график не созданы.");
        return;
    }

    // Отправка PNG
    bot.getApi().sendPhoto(chatId, InputFile::fromFile(pngFile, "image/png"),
        "График доступности IP Адреса: " + ip);

    // Отправка CSV
    bot.getApi().sendDocument(chatId, InputFile::fromFile(csvFile, "text/csv"),
        "Отчет по IP Адресу: " + ip);
}

void handleCallbackQuery(Bot& bot, CallbackQuery::Ptr query) {
    string data = query->data;
    long long chatId = query->from->id;

    if (data.find("ping_") == 0) {
        string ip = data.substr(5);

        if (sessions.count(chatId)) {
            bot.getApi().sendMessage(chatId, "Мониторинг уже запущен.");
            return;
        }

        Message::Ptr fakeMsg(new Message(*query->message));
        fakeMsg->text = "/ping " + ip;

        startPing(bot, fakeMsg);
    }
    else if (query->data.find("del_") == 0) {
        string ip = query->data.substr(4);

        InlineKeyboardMarkup::Ptr confirmKb(new InlineKeyboardMarkup);

        InlineKeyboardButton::Ptr yesBtn(new InlineKeyboardButton);
        yesBtn->text = "✅ Да";
        yesBtn->callbackData = "confirm_del_" + ip + "|" + to_string(query->message->messageId);  // <- передаём ID старого меню

        InlineKeyboardButton::Ptr noBtn(new InlineKeyboardButton);
        noBtn->text = "❌ Нет";
        noBtn->callbackData = "cancel|" + to_string(query->message->messageId);  // <- тоже

        confirmKb->inlineKeyboard.push_back({ yesBtn, noBtn });

        bot.getApi().sendMessage(query->from->id,
            "Вы уверены, что хотите удалить IP-адрес: " + ip + "? История мониторинга также автоматически удалится",
            false, 0, confirmKb);
    }
    else if (query->data.find("confirm_del_") == 0) {
        string payload = query->data.substr(12);
        size_t sep = payload.find('|');
        string ip = payload.substr(0, sep);
        int oldMsgId = stoi(payload.substr(sep + 1));
        long long chatId = query->from->id;

        // Удаляем сообщение с подтверждением и старое меню
        bot.getApi().deleteMessage(chatId, query->message->messageId);
        bot.getApi().deleteMessage(chatId, oldMsgId);

        // Проверяем, идёт ли мониторинг по этому IP
        {
            lock_guard<mutex> lock(sessionMutex);
            auto it = sessions.find(chatId);
            if (it != sessions.end() && it->second.ip == ip) {
                it->second.stopFlag = true;
                if (it->second.pingThread.joinable()) {
                    it->second.pingThread.join();
                }
                sessions.erase(it);
                bot.getApi().sendMessage(chatId, "🛑 Мониторинг IP " + ip + " был остановлен.");
            }
        }

        // Удаляем IP из БД
        if (removeMonitoredIp(chatId, ip)) {
            bot.getApi().sendMessage(chatId, "✅ IP " + ip + " удалён из мониторинга.");
        }
        else {
            bot.getApi().sendMessage(chatId, "❌ Не удалось удалить IP: " + ip);
            return;
        }

        // Перерисовываем меню IP
        sendIpList(bot, chatId);
    }

    else if (query->data.find("cancel|") == 0) {
        int oldMsgId = stoi(query->data.substr(7));
        long long chatId = query->from->id;

        // Удаляем подтверждение
        bot.getApi().deleteMessage(chatId, query->message->messageId);

        bot.getApi().sendMessage(chatId, "❎ Удаление отменено.");
    }

    else if (query->data == "cancel") {
        // Удалить сообщение с кнопками подтверждения
        bot.getApi().deleteMessage(query->message->chat->id, query->message->messageId);
        bot.getApi().sendMessage(chatId, "❎ Удаление отменено.");
    }
    else if (data.find("report_") == 0) {
        string ip = data.substr(7);

        // Создаём фейковое сообщение, чтобы вызвать handleReport
        Message::Ptr fakeMsg(new Message(*query->message));
        fakeMsg->text = "/report " + ip;

        handleReport(bot, fakeMsg);
    }
    else if (query->data == "stop_monitoring") {
        Message::Ptr fakeMsg(new Message(*query->message));
        fakeMsg->chat = query->message->chat;
        stopPing(bot, fakeMsg);
    }
    else if (data == "add_ip") {
        long long chatId = query->from->id;

        {
            lock_guard<mutex> lock(inputMutex);
            awaitingIpInput.insert(chatId);
        }

        bot.getApi().deleteMessage(chatId, query->message->messageId);
        bot.getApi().sendMessage(chatId, "Введите IP-адрес для добавления:");
    }
}

int main() {
    initDatabase();

    Bot bot("6338517569:AAF5WRHNL00b26aNP1NAroT0T5omOY-n6dw");

    // Запускаем диспетчер событий
    thread dispatcherThread(eventDispatcher);
    dispatcherThread.detach();

    bot.getEvents().onCommand("start", [&bot](Message::Ptr message) {
        handleStart(bot, message);
        handleListIp(bot, message);
        });

    bot.getEvents().onCommand("help", [&bot](Message::Ptr message) {
        initiateCommands(bot, message);
        });
    bot.getEvents().onCommand("add_ip", [&bot](Message::Ptr message) {
        handleAddIp(bot, message);
        });

    bot.getEvents().onCommand("list_ips", [&bot](Message::Ptr message) {
        handleListIp(bot, message);
        });

    bot.getEvents().onCommand("ping", [&bot](Message::Ptr message) {
        startPing(bot, message);
        });

    bot.getEvents().onCommand("stop", [&bot](Message::Ptr message) {
        stopPing(bot, message);
        });

    bot.getEvents().onCommand("report", [&bot](Message::Ptr message) {
        handleReport(bot, message);
        });

    bot.getEvents().onCallbackQuery([&bot](CallbackQuery::Ptr query) {
        handleCallbackQuery(bot, query);
        });
    bot.getEvents().onCommand("remove_ip", [&bot](Message::Ptr message) {
        handleRemove(bot, message);
        });
    bot.getEvents().onAnyMessage([&](Message::Ptr message) {
        long long chatId = message->chat->id;

        {
            lock_guard<mutex> lock(inputMutex);
            if (awaitingIpInput.count(chatId)) {
                awaitingIpInput.erase(chatId);
                string ip = message->text;

                if (ip.empty()) {
                    bot.getApi().sendMessage(chatId, "❌ IP-адрес не может быть пустым.");
                    return;
                }

                if (addMonitoredIp(chatId, ip)) {
                    bot.getApi().sendMessage(chatId, "✅ IP " + ip + " добавлен.");
                }
                else {
                    bot.getApi().sendMessage(chatId, "⚠️ IP " + ip + " уже в списке.");
                }

                sendIpList(bot, chatId);
                return;
            }
        }

        // Остальная логика обработки сообщений...
        });
    try {
        TgLongPoll longPoll(bot);
        cout << "Bot started. Username: " << bot.getApi().getMe()->username << endl;
        while (true) {
            longPoll.start();
        }
    }
    catch (exception& e) {
        cerr << "Bot error: " << e.what() << endl;
    }

    dispatcherThread.join();
    return 0;
}

 //"6338517569:AAF5WRHNL00b26aNP1NAroT0T5omOY-n6dw");