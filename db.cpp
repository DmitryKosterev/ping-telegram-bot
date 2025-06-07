#include "db.hpp"
#include <sqlite3.h>
#include <iostream>
#include <fstream>

using namespace std;

// --- Инициализация базы (создание таблиц, если их нет)
void initDatabase() {
    sqlite3* db;
    char* errMsg = nullptr;

    if (sqlite3_open("ping_bot.db", &db)) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        return;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, 0, &errMsg) != SQLITE_OK) {
        std::cerr << "DB FK error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }

    const char* sqlMonitored = R"(
        CREATE TABLE IF NOT EXISTS monitored_ips (
            chat_id INTEGER,
            ip_address TEXT,
            PRIMARY KEY (chat_id, ip_address)  -- Объединенный первичный ключ
        );
    )";

    const char* sqlIncidents = R"(
        CREATE TABLE IF NOT EXISTS monitoring (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            chat_id INTEGER,
            ip_address TEXT,
            date TEXT,
            time TEXT,
            status TEXT,
            FOREIGN KEY (chat_id, ip_address) REFERENCES monitored_ips(chat_id, ip_address) ON DELETE CASCADE
        );
    )";

    if (sqlite3_exec(db, sqlMonitored, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        cerr << "DB error monitored_ips table: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
    if (sqlite3_exec(db, sqlIncidents, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        cerr << "DB error incidents table: " << errMsg << endl;
        sqlite3_free(errMsg);
    }

    sqlite3_close(db);
}

void logIncident(long long chatId, const string& ip, const string& status) {
    sqlite3* db;
    char* errMsg = nullptr;

    if (sqlite3_open("ping_bot.db", &db)) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        return;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, 0, &errMsg) != SQLITE_OK) {
        cerr << "DB FK error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }

    sqlite3_stmt* stmt;
    string sql = "INSERT INTO monitoring (chat_id, ip_address, date, time, status) VALUES (?, ?, date('now'), time('now'), ?);";

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "SQL prepare error: " << sqlite3_errmsg(db) << endl;
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_int64(stmt, 1, chatId);
    sqlite3_bind_text(stmt, 2, ip.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, status.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        cerr << "SQL step error: " << sqlite3_errmsg(db) << endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}


// Получить список IP из monitored_ips для конкретного chat_id
vector<string> getMonitoredIps(long long chatId) {
    vector<string> ips;
    sqlite3* db;
    char* errMsg = nullptr;

    if (sqlite3_open("ping_bot.db", &db)) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        return ips;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, 0, &errMsg) != SQLITE_OK) {
        cerr << "DB FK error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return ips;
    }
    string sql = "SELECT ip_address FROM monitored_ips WHERE chat_id = ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, chatId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) ips.push_back(reinterpret_cast<const char*>(text));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ips;
}

// Добавить IP в monitored_ips для конкретного chat_id
bool addMonitoredIp(long long chatId, const string& ip) {
    sqlite3* db;
    char* errMsg = nullptr;

    if (sqlite3_open("ping_bot.db", &db)) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        return false;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, 0, &errMsg) != SQLITE_OK) {
        cerr << "DB FK error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    // Проверка на дубликат
    string sqlCheck = "SELECT COUNT(*) FROM monitored_ips WHERE chat_id = ? AND ip_address = ?;";
    sqlite3_stmt* stmtCheck;
    sqlite3_prepare_v2(db, sqlCheck.c_str(), -1, &stmtCheck, nullptr);
    sqlite3_bind_int64(stmtCheck, 1, chatId);
    sqlite3_bind_text(stmtCheck, 2, ip.c_str(), -1, SQLITE_STATIC);

    int count = 0;
    if (sqlite3_step(stmtCheck) == SQLITE_ROW) {
        count = sqlite3_column_int(stmtCheck, 0);
    }
    sqlite3_finalize(stmtCheck);

    if (count > 0) {
        sqlite3_close(db);
        return false; // Уже есть такой IP
    }

    string sqlInsert = "INSERT INTO monitored_ips (chat_id, ip_address) VALUES (?, ?);";
    sqlite3_stmt* stmtInsert;
    sqlite3_prepare_v2(db, sqlInsert.c_str(), -1, &stmtInsert, nullptr);
    sqlite3_bind_int64(stmtInsert, 1, chatId);
    sqlite3_bind_text(stmtInsert, 2, ip.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmtInsert) == SQLITE_DONE);

    sqlite3_finalize(stmtInsert);
    sqlite3_close(db);
    return success;
}

// Выгрузка БД в CSV
void getDataFromDatabase(const string& filename) {
    ofstream file(filename, ios::trunc);
    if (!file.is_open()) {
        cerr << "Failed to open file for writing\n";
        return;
    }

    sqlite3* db;
    char* errMsg = nullptr;

    if (sqlite3_open("ping_bot.db", &db)) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        return;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, 0, &errMsg) != SQLITE_OK) {
        cerr << "DB FK error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }

    file << "ip_address,date,time,status\n";

    string sql = "SELECT ip_address, date, time, status FROM monitoring;";
    sqlite3_exec(db, sql.c_str(), [](void* data, int argc, char** argv, char**) -> int {
        ofstream* file = reinterpret_cast<ofstream*>(data);
        for (int i = 0; i < argc; ++i) {
            *file << (argv[i] ? argv[i] : "NULL");
            if (i < argc - 1) *file << ",";
        }
        *file << "\n";
        return 0;
        }, &file, nullptr);

    sqlite3_close(db);
    file.close();
}

void getDataFromDatabaseByIp(const string& filename, long long chatId, const string& ip) {
    ofstream file(filename, ios::trunc);
    if (!file.is_open()) {
        cerr << "Failed to open file for writing\n";
        return;
    }

    sqlite3* db;
    char* errMsg = nullptr;

    if (sqlite3_open("ping_bot.db", &db)) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        return;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, 0, &errMsg) != SQLITE_OK) {
        cerr << "DB FK error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return;
    }

    file << "ip_address,date,time,status\n";

    string sql = R"(
        SELECT ip_address, date, time, status
        FROM monitoring
        WHERE chat_id = ? AND ip_address = ?
        ORDER BY date, time;
    )";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, chatId);
    sqlite3_bind_text(stmt, 2, ip.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        file << sqlite3_column_text(stmt, 0) << ","
            << sqlite3_column_text(stmt, 1) << ","
            << sqlite3_column_text(stmt, 2) << ","
            << sqlite3_column_text(stmt, 3) << "\n";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    file.close();
}
bool removeMonitoredIp(long long chatId, const string& ip) {
    sqlite3* db;
    char* errMsg = nullptr;

    if (sqlite3_open("ping_bot.db", &db)) {
        cerr << "DB open error: " << sqlite3_errmsg(db) << endl;
        return false;
    }
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, 0, &errMsg) != SQLITE_OK) {
        cerr << "DB FK error: " << errMsg << endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql = R"(
        
        DELETE FROM monitored_ips WHERE chat_id = ? AND ip_address = ?;)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        // Ошибка при подготовке запроса
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int64(stmt, 1, chatId);
    sqlite3_bind_text(stmt, 2, ip.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        // Ошибка при выполнении запроса
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }

    bool success = (sqlite3_changes(db) > 0);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return success;
}
