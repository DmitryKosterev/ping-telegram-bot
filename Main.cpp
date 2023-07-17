#include <iostream>
#include <string>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <sqlite3.h>
#include <tgbot/tgbot.h>
#include <thread>
#pragma warning(disable : 4996)
using namespace std;
using namespace TgBot;

bool pingDevice(const string& device) {
#ifdef _WIN32
    string command = "ping -n 1 " + device;  // Для Windows
#else
    string command = "ping -c 1 " + device; // Для Linux и Mac
#endif
    int result = system(command.c_str());
    return result == 0;
}
bool checkingDevice = true;

void logIncident(string& ipAdress, string& status);

void pingFunction(Bot& bot, string& deviceIP, Message::Ptr message) {
    setlocale(LC_ALL, "RUS");
    int i = 0;
    // Бесконечный цикл для проверки доступности устройства
    while (checkingDevice) {
        try {
            auto start = chrono::system_clock::now();
            if (pingDevice(deviceIP)) {
                if (i > 0) {
                    i = 0;
                    string status = u8"Подключение восстановлено";
                    bot.getApi().sendMessage(message->chat->id, status);
                    logIncident(deviceIP, status);
                }
                auto end = chrono::system_clock::now();
                chrono::duration<double> elapsedTime = end - start;
                chrono::duration<double> delayTime = 5000ms;
                cout << to_string(elapsedTime.count());
                if (elapsedTime > delayTime) {
                    i++;
                    string status = u8"Снизилась скорость передачи пакетов";
                    if (i <= 1) {

                        cout << endl << status << endl;
                        bot.getApi().sendMessage(message->chat->id, status);
                        logIncident(deviceIP, status);
                    }
                    else {
                        bot.getApi().sendMessage(message->chat->id, status);
                    }
                }
            }
            else {
                i++;
                if (i <= 1) {
                    string status = "Не отвечает";
                    cout << endl << "Устройство не отвечает " << i << "минуту" << endl;
                    bot.getApi().sendMessage(message->chat->id, u8"Устройство недоступно!");
                    logIncident(deviceIP, status);
                }
                else {
                    cout << endl << "Устройство не отвечает " << i << "минуту" << endl;
                    bot.getApi().sendMessage(message->chat->id, u8"Устройство недоступно!");
                }
            }

            // Пауза в 60 секунд перед следующей проверкой
            this_thread::sleep_for(chrono::seconds(60));
        }
        catch (exception& e) {
            //cout << "Ping: " << e.what() << endl;
        }
    }
}


static int callback(void* data, int argc, char** argv, char** azColName) {
    ofstream outputFile("incidents.csv"); // Создаем файл для записи данных таблицы

    // Записываем заголовки столбцов
    for (int i = 0; i < argc; i++) {
        outputFile << azColName[i];
        if (i < argc - 1) {
            outputFile << ",";
        }
    }
    outputFile << endl;

    // Записываем данные таблицы
    for (int i = 0; i < argc; i++) {
        outputFile << argv[i];
        if (i < argc - 1) {
            outputFile << ",";
        }
    }
    outputFile << endl;

    outputFile.close();

    return 0;
}

// Функция для получения данных из базы данных и записи их в файл
void getDataFromDatabase() {
    // Открываем соединение с базой данных
    sqlite3* db;
    int rc = sqlite3_open("incidents.db", &db);
    if (rc) {
        cerr << "Ошибка при открытии базы данных: " << sqlite3_errmsg(db) << endl;
        return;
    }

    // Выполняем SQL-запрос для получения данных из таблицы
    string sql = "SELECT * FROM incidents;";
    rc = sqlite3_exec(db, sql.c_str(), callback, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "Ошибка при выполнении SQL-запроса: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    // Закрываем соединение с базой данных
    sqlite3_close(db);
}
void logIncident(string& ipAdress, string& status) {
    sqlite3* db;
    int rc = sqlite3_open("incidents.db", &db);
    if (rc) {
        cout << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return;
    }

    string insertQuery = "INSERT INTO incidents (ip_address, date, time, status) VALUES ('" + ipAdress + "', date('now'), time('now'), '" + status + "')";
    rc = sqlite3_exec(db, insertQuery.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        cout << "SQL error: " << sqlite3_errmsg(db) << endl;
    }

    sqlite3_close(db);
}
int main() {
    // Инициализируем бота
    Bot bot("YOUR TOKEN");
    string chatId;
    // /start
    bot.getEvents().onCommand("start", [&bot](Message::Ptr message) {
        checkingDevice = true;
    bot.getApi().sendMessage(message->chat->id, u8"Добро пожаловать. Вот - краткая инструкция по командам:\n"
        u8"/help - показать список команд\n"
        u8"/ping - пинговать устройство, необходимо вводить '/ping ip-adress'. Вводить сразу, а не отправлять потом, иначе бот вылетит.\n"
        u8"/stop - остановить пинг\n"
        u8"/download - скачать таблицу incidents");
        });

    // /stop
    bot.getEvents().onCommand("stop", [&bot](Message::Ptr message) {
        checkingDevice = false;
    bot.getApi().sendMessage(message->chat->id, u8"Ping остановлен");
    checkingDevice = true;
        });
    // /help
    bot.getEvents().onCommand("help", [&bot](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, u8"Список доступных команд:\n"
        u8"/help - показать список команд\n"
            u8"/ping - пинговать устройство\n"
            u8"/stop - остановить пингование\n"
            u8"/download - скачать таблицу incidents");
        });
    // /statistic
    bot.getEvents().onCommand("download", [&bot](Message::Ptr message) {
        // Получаем данные из базы данных и записываем их в файл
        getDataFromDatabase();
    // Отправляем файл с данными таблицы пользователю
    bot.getApi().sendDocument(message->chat->id, InputFile::fromFile("incidents.csv", "cvs"));
        });
    // /ping
    bot.getEvents().onCommand("ping", [&bot](Message::Ptr message) {
        //bot.getApi().sendMessage(message->chat->id, u8"Введите Ip-адресс устройства:");
        //bot.getEvents().onAnyMessage([&bot](Message::Ptr message) {
        string deviceIP = message->text.substr(6);
    bot.getApi().sendMessage(message->chat->id, u8"Автоматический контроль за устройством по адресу: " + deviceIP + u8" запущен.\n Чтобы его остановить воспользуйтесь командой /stop");
    // Запуск функции в отдельном потоке
    thread pingThread(pingFunction, ref(bot), ref(deviceIP), message);
    pingThread.detach();
    //});
        });
    // Запускаем бота
    try {
        cout << "Bot username: " << bot.getApi().getMe()->username << endl;
        TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
        }
    }
    catch (exception& e) {
        cout << "Bot error: " << e.what() << endl;
    }

    return 0;
}
