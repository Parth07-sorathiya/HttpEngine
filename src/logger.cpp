#include<logger.h>
#include<fstream>
#include<iostream>
#include <ctime>
#include<string>
using namespace std;
Logger::Logger(){
}

Logger::Logger(string filename, LogLevel logLevel){
    logFile.open(filename, std::ios::app);
    if (!logFile.is_open()) {
        cerr << "Failed to open log file\n";
        exit(1);
    }
    level = logLevel;
}

void Logger::log(const string &message,LogLevel currLevel = INFO){
    const char* levelStr = (currLevel == INFO ? "INFO" : (currLevel == WARN ? "WARN" : (currLevel == DEBUG ? "DEBUG": "ERROR")));
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    string formatted = "[" + string(buf) + "] [" + levelStr + "] " + message;
    logFile << formatted << endl;
    if(currLevel >= level){
        cout << formatted << endl;
    }
}

void Logger::logInfo(const string &message){
    log(message, INFO);
}

void Logger::logError(const string &message){
    log(message, ERROR);
}

void Logger::logWarn(const string &message){
    log(message, WARN);
}

void Logger::logDebug(const string &message){
    log(message, DEBUG);
}


Logger::~Logger(){
    if(logFile.is_open()){
        logFile.close();
    }
}
