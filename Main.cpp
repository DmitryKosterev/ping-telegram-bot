#include <string>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <sqlite3.h>
#include <tgbot/tgbot.h>
#include <thread>
#include <unordered_map>
#include <atomic>

#pragma warning(disable : 4996)

using namespace std;
using namespace TgBot;

// Количество запущенных потоков
int numThreads = 0;

// Мапа для хранения всех запущенных потоков
unordered_map<int, thread> threads;

// Массив флагов для остановки каждого потока
atomic<bool> stopFlags[10];

// Мапа для хранения соответствия между чат айди и номером потока
unordered_map<long long, int> chatToThread;

string deviceIP;

bool pingDevice(const string& device) {
#ifdef _WIN32
    string command = "ping -n 1 " + device;  // для Windows
#else
    string command = "ping -c 1 " + device; // для Linux и Mac
#endif
    int result = system(command.c_str());
    return result == 0;
}


void logIncident(string& ipAdress, string& status);

void pingFunction(Bot& bot, Message::Ptr message, const string& ip, int threadNum, atomic<bool>& stopFlag) {
    setlocale(LC_ALL, "RUS");
    int i = 0;
    // Бесконечный цикл для проверки доступности устройства
    while (!stopFlag) {
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


                    string status = u8"Не отвечает";
                    logIncident(deviceIP, status);
                    cout << endl << "Устройство не отвечает " << i << " минуту" << endl;
                    bot.getApi().sendMessage(message->chat->id, u8"Устройство недоступно!");

                }
                else {
                    cout << endl << "Устройство не отвечает " << i << " минут(ы)" << endl;
                    bot.getApi().sendMessage(message->chat->id, u8"Устройство недоступно!");
                }
            }

            // Пауза в 60 секунд перед следующей проверкой
            this_thread::sleep_for(chrono::seconds(60));
        }
        catch (exception& e) {
            cout << "Ping: " << e.what() << endl;
        }
    }
}

//  /ping
void handleStartPingCommand(Bot& bot, Message::Ptr message) {
    // создание нового потока и добавление его номера в мапу
    try {
        deviceIP = message->text.substr(6);
        bot.getApi().sendMessage(message->chat->id, u8"Автоматический контроль за устройством по адресу: " + deviceIP + u8" запущен.\n Чтобы его остановить воспользуйтесь командой /stop");
        int threadNum = numThreads++;
        threads[threadNum] = thread(pingFunction, ref(bot), message, deviceIP, threadNum, ref(stopFlags[threadNum]));
        // связь chatId с номером потока
        chatToThread[message->chat->id] = threadNum;
    }
    catch (exception& e) {
        cout << "Bot: " << e.what() << endl;
        bot.getApi().sendMessage(message->chat->id, u8"������� �� ������� '/ping ip_adress'");
    }
}

// /stop
void handleStopPingCommand(Bot& bot, Message::Ptr message) {
    // получение номера потока из мапы и установка флага остановки для этого потока
    if (chatToThread.count(message->chat->id) > 0) {
        int threadNum = chatToThread[message->chat->id];
        stopFlags[threadNum] = true;
        threads[threadNum].join();
        threads.erase(threadNum);
        chatToThread.erase(message->chat->id);
        cout << endl << "Thread " + to_string(threadNum) + " stopped" << endl;
        bot.getApi().sendMessage(message->chat->id, u8"Выполнение команды '/ping' прекращено");
    }
    else {
        bot.getApi().sendMessage(message->chat->id, u8"Команда '/ping' не была запущена");
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
// Функция для записи в файл
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
    Bot bot("6338517569:AAFhOjUsqB3zUG-vMGvWHje-gAqSOiU8v2Y");
    string chatId;
    setlocale(LC_ALL, "RUS");
    // /start
    bot.getEvents().onCommand("start", [&bot](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, u8"Добро пожаловать. Вот - краткая инструкция по командам:\n"
        u8"/help - показать список команд\n"
            u8"/ping - пинговать устройство, необходимо вводить '/ping ip-adress'. Вводить сразу, а не отправлять потом, иначе бот вылетит.\n"
            u8"/stop - остановить пинг\n"
            u8"/download - скачать таблицу incidents"
        );
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
    bot.getEvents().onCommand("ping", bind(handleStartPingCommand, ref(bot), placeholders::_1));
    // /stop
    bot.getEvents().onCommand("stop", bind(handleStopPingCommand, ref(bot), placeholders::_1));


    // Запускаем бота
    try {
        cout << "Bot username: " << bot.getApi().getMe()->username << endl;
        TgLongPoll longPoll(bot);
        while (true) {
            longPoll.start();
        }
    }
    catch (exception& e) {
        cout << "Bot error: " << e.what() << endl << "chatId:" << chatId << endl;
    }

    return 0;
}