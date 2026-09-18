#pragma once
#include<fstream>
#include<string>


enum LogLevel{
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger{
    private:
        std::ofstream logFile;
        LogLevel level;
    public:
        Logger();
        Logger(std::string filename, LogLevel logLevel);
        void log(const std::string &message,LogLevel currLevel);
        void logInfo(const std::string &message);
        void logError(const std::string &message);
        void logWarn(const std::string &message);
        void logDebug(const std::string &message);
        ~Logger();
};