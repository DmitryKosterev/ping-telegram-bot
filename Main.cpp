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
    string command = "ping -n 1 " + device;  // ��� Windows
#else
    string command = "ping -c 1 " + device; // ��� Linux � Mac
#endif
    int result = system(command.c_str());
    return result == 0;
}
bool checkingDevice = true;

void logIncident(string& ipAdress, string& status);

void pingFunction(Bot& bot, string& deviceIP, Message::Ptr message) {
    setlocale(LC_ALL, "RUS");
    int i = 0;
    // ����������� ���� ��� �������� ����������� ����������
    while (checkingDevice) {
        try {
            auto start = chrono::system_clock::now();
            if (pingDevice(deviceIP)) {
                if (i > 0) {
                    i = 0;
                    string status = u8"����������� �������������";
                    bot.getApi().sendMessage(message->chat->id, status);
                    logIncident(deviceIP, status);
                }
                auto end = chrono::system_clock::now();
                chrono::duration<double> elapsedTime = end - start;
                chrono::duration<double> delayTime = 5000ms;
                cout << to_string(elapsedTime.count());
                if (elapsedTime > delayTime) {
                    i++;
                    string status = u8"��������� �������� �������� �������";
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
                    string status = "�� ��������";
                    cout << endl << "���������� �� �������� " << i << "������" << endl;
                    bot.getApi().sendMessage(message->chat->id, u8"���������� ����������!");
                    logIncident(deviceIP, status);
                }
                else {
                    cout << endl << "���������� �� �������� " << i << "������" << endl;
                    bot.getApi().sendMessage(message->chat->id, u8"���������� ����������!");
                }
            }

            // ����� � 60 ������ ����� ��������� ���������
            this_thread::sleep_for(chrono::seconds(60));
        }
        catch (exception& e) {
            //cout << "Ping: " << e.what() << endl;
        }
    }
}


static int callback(void* data, int argc, char** argv, char** azColName) {
    ofstream outputFile("incidents.csv"); // ������� ���� ��� ������ ������ �������

    // ���������� ��������� ��������
    for (int i = 0; i < argc; i++) {
        outputFile << azColName[i];
        if (i < argc - 1) {
            outputFile << ",";
        }
    }
    outputFile << endl;

    // ���������� ������ �������
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

// ������� ��� ��������� ������ �� ���� ������ � ������ �� � ����
void getDataFromDatabase() {
    // ��������� ���������� � ����� ������
    sqlite3* db;
    int rc = sqlite3_open("incidents.db", &db);
    if (rc) {
        cerr << "������ ��� �������� ���� ������: " << sqlite3_errmsg(db) << endl;
        return;
    }

    // ��������� SQL-������ ��� ��������� ������ �� �������
    string sql = "SELECT * FROM incidents;";
    rc = sqlite3_exec(db, sql.c_str(), callback, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        cerr << "������ ��� ���������� SQL-�������: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    // ��������� ���������� � ����� ������
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
    // �������������� ����
    Bot bot("YOUR TOKEN");
    string chatId;
    // /start
    bot.getEvents().onCommand("start", [&bot](Message::Ptr message) {
        checkingDevice = true;
    bot.getApi().sendMessage(message->chat->id, u8"����� ����������. ��� - ������� ���������� �� ��������:\n"
        u8"/help - �������� ������ ������\n"
        u8"/ping - ��������� ����������, ���������� ������� '/ping ip-adress'. ������� �����, � �� ���������� �����, ����� ��� �������.\n"
        u8"/stop - ���������� ����\n"
        u8"/download - ������� ������� incidents");
        });

    // /stop
    bot.getEvents().onCommand("stop", [&bot](Message::Ptr message) {
        checkingDevice = false;
    bot.getApi().sendMessage(message->chat->id, u8"Ping ����������");
    checkingDevice = true;
        });
    // /help
    bot.getEvents().onCommand("help", [&bot](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, u8"������ ��������� ������:\n"
        u8"/help - �������� ������ ������\n"
            u8"/ping - ��������� ����������\n"
            u8"/stop - ���������� ����������\n"
            u8"/download - ������� ������� incidents");
        });
    // /statistic
    bot.getEvents().onCommand("download", [&bot](Message::Ptr message) {
        // �������� ������ �� ���� ������ � ���������� �� � ����
        getDataFromDatabase();
    // ���������� ���� � ������� ������� ������������
    bot.getApi().sendDocument(message->chat->id, InputFile::fromFile("incidents.csv", "cvs"));
        });
    // /ping
    bot.getEvents().onCommand("ping", [&bot](Message::Ptr message) {
        //bot.getApi().sendMessage(message->chat->id, u8"������� Ip-������ ����������:");
        //bot.getEvents().onAnyMessage([&bot](Message::Ptr message) {
        string deviceIP = message->text.substr(6);
    bot.getApi().sendMessage(message->chat->id, u8"�������������� �������� �� ����������� �� ������: " + deviceIP + u8" �������.\n ����� ��� ���������� �������������� �������� /stop");
    // ������ ������� � ��������� ������
    thread pingThread(pingFunction, ref(bot), ref(deviceIP), message);
    pingThread.detach();
    //});
        });
    // ��������� ����
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