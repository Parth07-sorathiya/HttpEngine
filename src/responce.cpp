#include<response.h>
#include<string.h>
#include<sys/socket.h>
#include<logger.h>
#include<unistd.h>
#include<connection.h>
#include<iostream>
using namespace std;

Response::Response(int socketFileDescr): socketFd(socketFileDescr){
    httpVersion = "HTTP/1.1";
    statusCode = 200;
    statusMessage = "OK";
    body = "";
    //Default headers
    setHeader("Server", "mini-nginx/1.0");
}


string Response::getMessageForStatusCode(int code){
    switch(code){
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        case 503:
            return "Service Unavailable";
        default:
            return "Unknown Status";
    }
}

void Response::setHeader(const std::string &key, const std::string &value){
    headers[key] = value;
}
int Response::sendResponse(){
    //construction of response string
    // Ensure mandatory headers are set BEFORE composing
    if (!body.empty()) {
        setHeader("Content-Length", std::to_string(body.size()));
        setHeader("Content-Type", "text/plain");
    } else {
        setHeader("Content-Length", "0");
    }

    if(conn->isKeepAliveConnection()){
        setHeader("Connection", "keep-alive");
    }else{
        setHeader("Connection", "close");
    }

    response = httpVersion + " " + to_string(statusCode) + " " + statusMessage + "\r\n";
    for(auto &headerPair: headers){
        response += headerPair.first + ": " + headerPair.second + "\r\n";   
    }

    //End of headersconn.enableEpollin()
    response += "\r\n"; 
    if(!body.empty()){
        response += body;
    }
   
    conn->responeIsReady();
    return 1;
}
void Response::setStatus(int code){
    statusCode = code;
    statusMessage = getMessageForStatusCode(code);
}

void Response::setBody(const string &bodyContent){
    body = bodyContent;
}

void Response::setMessage(const std::string &message){
    statusMessage = message;
}

void Response::clear(){
    httpVersion = "HTTP/1.1";
    statusCode = 200;
    statusMessage = "OK";
    body.clear();
    response.clear();
    headers.clear();
    setHeader("Server", "mini-nginx/1.0");
}