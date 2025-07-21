#include "logger.h"
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

static std::string shortenPath(const std::string& filePath) {
    try {
        fs::path p(filePath);
        fs::path workDir = fs::current_path();
        fs::path relPath = fs::relative(p, workDir);
        return relPath.string();
    }
    catch (const fs::filesystem_error&) {
        return filePath;
    }
}

Logger::Logger(const std::string& filename) : logfile(filename, std::ios::app) {
    if (!logfile) {
        std::cerr << "Error on opening log-file: " << filename << std::endl;
    }
}

Logger::~Logger() {
    if (logfile.is_open()) {
        logfile.close();
    }
}

void Logger::logDebug(const std::string& message, const std::string& file, int line) {
    std::string formattedMessage = "[DEBUG] [" + shortenPath(file) + ":" + std::to_string(line) + "] " + message;
    logfile << formattedMessage << std::endl;
}

void Logger::logInfo(const std::string& message, const std::string& file, int line) {
    std::string formattedMessage = "[INFO] [" + shortenPath(file) + ":" + std::to_string(line) + "] " + message;
    std::cout << formattedMessage << std::endl;
    logfile << formattedMessage << std::endl;
}

void Logger::logWarning(const std::string& message, const std::string& file, int line) {
    std::string formattedMessage = "[WARNING] [" + shortenPath(file) + ":" + std::to_string(line) + "] " + message;
    std::cout << "\033[33m" << formattedMessage << "\033[0m" << std::endl;
    logfile << formattedMessage << std::endl;
}

void Logger::logError(const std::string& message, const std::string& file, int line) {
    std::string formattedMessage = "[ERROR] [" + shortenPath(file) + ":" + std::to_string(line) + "] " + message;
    std::cout << "\033[31m" << formattedMessage << "\033[0m" << std::endl;
    logfile << formattedMessage << std::endl;
}
