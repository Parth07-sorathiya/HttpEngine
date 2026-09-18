#pragma once
#include <stdexcept>
#include <string>

class HttpException : public std::runtime_error {
public:
    explicit HttpException(const std::string &msg)
        : std::runtime_error(msg) {}
};

class BadRequestException : public HttpException {
public:
    explicit BadRequestException(const std::string &msg)
        : HttpException("Bad Request: " + msg) {}
};

class NotSupportedException : public HttpException {
public:
    explicit NotSupportedException(const std::string &msg)
        : HttpException("Not supported: " + msg) {}
};

class ConnectionClosedException : public HttpException {
public:
    explicit ConnectionClosedException(const std::string &msg)
        : HttpException("Connection Closed: " + msg) {}
};

class ReceiveErrorException : public HttpException {
public:
    explicit ReceiveErrorException(const std::string &msg)
        : HttpException("Receive Error: " + msg) {}
};