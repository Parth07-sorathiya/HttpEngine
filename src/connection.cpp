#include<connection.h>
#include<sys/epoll.h>
#include<string.h>
#include<iostream>
#include<unistd.h>
#include<algorithm>
#include<iostream>

using namespace std;
ClientConnection::ClientConnection(int clientFd, int epollInstanceFd){
    clientSocketFd = clientFd;
    res = Response(clientFd);
    res.conn = this;
    epollFd = epollInstanceFd;
}

bool ClientConnection::isKeepAliveConnection(){
   auto it = req.headers.find("connection");
   std::string val  =  it!=req.headers.end()?it->second: "";
   std::transform(val.begin(), val.end(), val.begin(), ::tolower);
   return !(it != req.headers.end() && val == "close");
}

bool ClientConnection::isRequestCompleted(){
    return isRequestReadingDone;
}

bool ClientConnection::isResponseCompleted(){
    return isResponseSendingDone;
}

//Make the connection object ready for new connection from the client
void ClientConnection::clearCurrentRequest(){
    isRequestReadingDone = false;
    isResponseSendingDone = false;
    req.clear();
    res.clear();
    requestBody.clear();
    requestHeaders.clear();
    responsePtr = nullptr;
    currentlySentBytes = 0;
    totalResponseLength = 0;
    res.socketFd = clientSocketFd;
    headerReceived = false;
    contentLength = 0;
    totalBodyBytesReceived = 0;
    state = ConnState::REQUEST_RESET;
}

void ClientConnection::responeIsReady(){
    responsePtr = res.response.c_str();
    totalResponseLength = res.response.size();
    int status = enableEpollout();
    if(status == -1){
        deleteFdFromEpoll();
        close(clientSocketFd);
    }
}

int ClientConnection::enableEpollin(){
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = clientSocketFd;
    int epollModStatus = epoll_ctl(epollFd, EPOLL_CTL_MOD, clientSocketFd, &ev);
    if(epollModStatus == -1){
        return -1;
    }
    return 1;
}

int ClientConnection::enableEpollout(){
    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLET;
    ev.data.fd = clientSocketFd;
    int epollModStatus = epoll_ctl(epollFd, EPOLL_CTL_MOD, clientSocketFd, &ev);
    if(epollModStatus == -1){
        return -1;

    }
    return 1;
}

int ClientConnection::addFdAndEnableEpollin(){
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = clientSocketFd;
    int epolAddStatus = epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSocketFd, &ev);
    if(epolAddStatus == -1){
        return -1;
    }
    return 1;
}

int ClientConnection::deleteFdFromEpoll(){
    int epollDelStatus = epoll_ctl(epollFd, EPOLL_CTL_DEL, clientSocketFd, NULL);
    if(epollDelStatus == -1){
        return -1;
    }
    return 1;
}

int ClientConnection::setEventForEpollEvent(struct epoll_event ev){
    int epollModStatus = epoll_ctl(epollFd, EPOLL_CTL_MOD, clientSocketFd, &ev);
    if(epollModStatus == -1){
        return -1;
    }
    return 1;
}