#pragma once
#include<string>
#include<unordered_map>
#include<vector>


class Request{
    private:
        void parseHeaders(std::vector<std::string> headersLines);
        void parseRequestLine(const std::string &requestLine);
        void parseQueryParams();

    public:
        std::string method;
        std::string uri;
        std::string path;
        std::string httpVersion;
        std::string body;
        std::unordered_map<std::string, std::string> headers;
        std::unordered_map<std::string, std::string> queryParams;
        std::unordered_map<std::string, std::string> params;
        Request(std::string headers, std::string body);
        Request() = default;
        void clear();
};