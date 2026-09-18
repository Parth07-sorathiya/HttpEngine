#pragma once
#include<string>
#include<logger.h>
#include<unordered_map>
#include<routeHandler.h>
#include<middleware.h>
#include<connection.h>
#include<threadpool.h>
#include<timeoutManager.h>
const std::string LOG_FILE = "logs/server.log";

//Main server class definition
class Server{
    private:
        Logger logger;
        std::vector<RouteHandler> routeHandlers;
        std::vector<middlewareFunction> middlewareFunctions;
        std::unordered_map<int, ClientConnection> connectionMap;
        ThreadPool threadPool;
        requestHandler requestHanlderFunction;
        int createServerSocket(int port);   
        void listenRequests(int serverSocket);
        void getIpStr(const struct sockaddr *sa, char *s, size_t maxlen);
        int receiveHttpRequest(ClientConnection &conn);
        handlerFunction getRouteHandlerForPath(Request &request);
        bool validatePath(std::string routePath, std::string requestPath,std::unordered_map<std::string, std::string> &params);
        int acceptClientConnections(int serverSocketFd, int epollFd);
        int sendHttpResponse(ClientConnection &conn);
        void closeConnection(ClientConnection &conn);
        TimeoutManager timeoutManager;
        void handleTimeouts();


    public:
        Server(int threadCount);
        void listen(int port);
        void route(const std::string &method, const std::string &path, handlerFunction &handler);
        // void use(middlewareFunction middleware);
        void get(const std::string &path, handlerFunction handler);
        void post(const std::string &path, handlerFunction handler);
        void put(const std::string &path, handlerFunction handler);
        void del(const std::string &path, handlerFunction handler);
        void use(middlewareFunction);
};
