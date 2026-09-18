#pragma once
#include <string.h>
#include <request.h>
#include <response.h>
#include <timeoutManager.h>

enum class ConnState {
    REQUEST_RESET,
    READING_REQUEST,
    PROCESSING,
    WRITING_RESPONSE,
    KEEP_ALIVE_WAIT
};


class ClientConnection{
    public:
        int clientSocketFd = -1;
        Request req;
        Response res;
        bool isRequestReadingDone = false;
        bool isResponseSendingDone = false;
        std::string requestHeaders;
        std::string requestBody;
        const char *responsePtr = NULL;
        int currentlySentBytes = 0;
        int totalResponseLength = 0;
        int epollFd;
        bool headerReceived = false;
        size_t contentLength = 0;
        size_t totalBodyBytesReceived = 0;
        // timeout tracking
        unsigned long long timerVersion = 0;
        ConnState state = ConnState::REQUEST_RESET;
        TimePoint processingStart;
        TimePoint lastActivity;


        ClientConnection(int clientFd, int epollInstanceFd);
         ClientConnection() = default;  
        bool isKeepAliveConnection();
        bool isRequestCompleted();
        bool isResponseCompleted();
        void clearCurrentRequest();
        void responeIsReady();
        int enableEpollin();
        int enableEpollout();
        int addFdAndEnableEpollin();
        int deleteFdFromEpoll();
        int setEventForEpollEvent(struct epoll_event ev);
};