#pragma once
#include<string>
#include<unordered_map>
#include<logger.h>

class ClientConnection;

class Response{
    public:
        std::string httpVersion;
        int statusCode;
        std::string statusMessage;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        int sendResponseStatus;
        int socketFd;
        ClientConnection *conn;
        std::string response;
        Response(int socketFd);
        Response()=default;
        void setHeader(const std::string &key, const std::string &value);
        int sendResponse();
        void setStatus(int code);
        void setBody(const std::string &bodyContent);
        void setMessage(const std::string &message);
        std::string getMessageForStatusCode(int code);
        void clear();
};