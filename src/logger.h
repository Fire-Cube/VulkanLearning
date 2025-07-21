#pragma once

#include <fstream>
#include <iostream>
#include <string>

class Logger {
public:
    Logger(const std::string& filename);
    ~Logger();

	void logDebug(const std::string& message, const std::string& file, int line);
    void logInfo(const std::string& message, const std::string& file, int line);
    void logWarning(const std::string& message, const std::string& file, int line);
    void logError(const std::string& message, const std::string& file, int line);

private:
    std::ofstream logfile;
};

extern Logger globalLogger;

#define LOG_DEBUG(message) (globalLogger.logDebug((message), __FILE__, __LINE__))
#define LOG_INFO(message) (globalLogger.logInfo((message), __FILE__, __LINE__))
#define LOG_WARNING(message) (globalLogger.logWarning((message), __FILE__, __LINE__))
#define LOG_ERROR(message) (globalLogger.logError((message), __FILE__, __LINE__))
