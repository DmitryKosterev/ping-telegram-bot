#pragma once
#include <string>
#include <vector>

void initDatabase();
void logIncident(long long chatId, const std::string& ip, const std::string& status);
bool addMonitoredIp(long long chatId, const std::string& ip);
std::vector<std::string> getMonitoredIps(long long chatId);
void getDataFromDatabase(const std::string& filename);
void getDataFromDatabaseByIp(const std::string& filename, long long chatId, const std::string& ip);
bool removeMonitoredIp(long long chatId, const std::string& ip);