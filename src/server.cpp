#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <logger.h>
#include <server.h>
#include <algorithm>
#include <request.h>
#include <exceptions.h>
#include <response.h>
#include <sstream>
#include <middleware.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <threadpool.h>
#include <timeoutManager.h>

using namespace std;
//How many pending connections queue will hold
const int BACKLOG = 4096;
//MAX_EPOLL_EVENTS specify the max number of events the epoll wait can return, if there are more, the epoll wait will return them in subsequenct calls
const int MAX_EPOLL_EVENTS = 2048;
extern vector<std::string> SUPPORTED_METHODS;

Server::Server(int threadCount): logger(LOG_FILE, ERROR), threadPool(threadCount), timeoutManager(TimeoutConfig()) {
    requestHanlderFunction = [this](ClientConnection &conn){
        //Once we got complete request, let a worker thread handle the request handling. 
        try{
            //parsing etc will happen
            conn.req = Request(conn.requestHeaders, conn.requestBody);
        }catch(exception &e){
            //Send a bad request error
            conn.res.setStatus(400);
            conn.res.sendResponse();
            return;
        }

        //Match the request to registered routes and do error handling if not found
        handlerFunction routeHandler = getRouteHandlerForPath(conn.req);
        //Invalid request route
        if(!routeHandler){
            // send 404 response
            conn.res.setStatus(404);
            conn.res.setBody("No handler found for route: " + conn.req.path);
            conn.res.sendResponse();
             return;
        }

        // Handle the middlewares here
        for(auto middlewareFunc: middlewareFunctions){
            try {
                middlewareFunc(conn.req, conn.res);
            } catch (std::exception& e) {
                //send an error to the client
                conn.res.setStatus(400);
                conn.res.setBody("Error occured during the execution of middleware function" + std::string(e.what()));
                conn.res.sendResponse();
                return;
            }
        }

        //Call the respective handler functions and handler will send the response back
        try {
            routeHandler(conn.req, conn.res);
        }catch (std::exception& e) {
            //Handle exception while handler function. 
            conn.res.setStatus(500);
            conn.res.setBody("Function threw exception: " + std::string(e.what()));
            conn.res.sendResponse();
        }
    };
}

int Server::acceptClientConnections(int serverSocketFd, int epollFd){
        struct sockaddr_storage clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSoc = accept(serverSocketFd, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if(clientSoc == -1){
            logger.logError("Unable to create a client socket" + string(strerror(errno)));
            return -1;
        }
        //Make the client socket non blocking as well. This will make the recv and send non blocking. 
        int flags = fcntl(clientSoc, F_GETFL);
        fcntl(clientSoc, F_SETFL, flags | O_NONBLOCK);

        //Print the client IP
        struct sockaddr *clientAddrPtr = (struct sockaddr *)&clientAddr;
        int addressLen = clientAddrPtr->sa_family== AF_INET? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
        char ip[addressLen];
        getIpStr(clientAddrPtr, ip, addressLen);
        logger.logInfo("Accepted new connection from " + string(ip));

        return clientSoc;
}

void Server:: listenRequests(int serverSocketFd){
    //create a epollInstance, pass any value greater than 0, that will be ignored anyway.
    int epollFd = epoll_create(1);
    if(epollFd  == -1){
        logger.logError("Unable to create a epoll instance: " + string(strerror(errno)));
        exit(1);
    }

    //add the serversocket to epoll to get read events which is nothing but a new connection request
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = serverSocketFd;

    int epollAddStatus = epoll_ctl(epollFd, EPOLL_CTL_ADD, serverSocketFd, &ev);
    if(epollAddStatus == -1){
        logger.logError("Unable to add server socket to epoll instance: " + string(strerror(errno)));
        exit(1);
    }

    struct epoll_event events[MAX_EPOLL_EVENTS];
    //This is the reactor, it reacts to different events that happens during the life cycle of a socekt connetion of different sockets
    while(true){
        //now we are ready to get the events. If the event is for server socket, that is a new connections, we will accept. If the event is for client sockets, the based on read or write, we perform corresponsing action on that socket. 
        int timeoutInMs = 2500;        
        int eventCount = epoll_wait(epollFd, events, MAX_EPOLL_EVENTS, timeoutInMs);
        if(eventCount < 0 ){
            //eventCount = -1;
            if(errno==EINTR){
                //interruped by a signal handler
                continue;
            }else{
                logger.logError("Error occured during epoll_wait: " + string(strerror(errno)));
                exit(1);
            }
        }

        handleTimeouts();
        if(eventCount == 0){
            //timer timeout occured
            continue;
        }

        //when eventCount > 0, the there are events and we need to process them
        for(int i=0; i<eventCount; i++){
            int eventFd = events[i].data.fd;
            if(eventFd == serverSocketFd){
                //accept the connections here
                //continiously accetp when many are available to connect. 
                while (true) {
                    int clientSocFd = acceptClientConnections(serverSocketFd, epollFd);
                    if (clientSocFd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; // all accepted
                        logger.logError("Error accepting new connection: " + string(strerror(errno)));
                        break;
                    }
                    logger.logInfo("Newly created client socket " + to_string(clientSocFd));
                    auto& connRef = connectionMap.try_emplace(clientSocFd, clientSocFd, epollFd).first->second;
                    if (connRef.addFdAndEnableEpollin() == -1) {
                        logger.logError("Unable to add client socket to epoll: " + string(strerror(errno)) + " "+ to_string(clientSocFd));
                        closeConnection(connRef);
                    }
                    
                }
                continue;
            }else{
                //process read or write request for the client sockets
                //Get the connection object
                ClientConnection &conn = connectionMap[eventFd];
                if(events[i].events & (EPOLLERR | EPOLLHUP)){
                    closeConnection(conn);
                    continue;
                }
                if(events[i].events & EPOLLIN){
                    //recv the data from the client
                    if(conn.state == ConnState::REQUEST_RESET){
                        conn.state  = ConnState::READING_REQUEST;
                        conn.lastActivity = Clock::now();
                        conn.timerVersion++;
                        timeoutManager.refresh(conn.clientSocketFd, conn.timerVersion, TimeoutType::READ);
                    }
                    int recvStatus = receiveHttpRequest(conn);
                    if(recvStatus <= 0){
                        if(recvStatus == -2){
                            //Send a bad request error
                            conn.res.setStatus(400);
                            conn.res.sendResponse();
                        }else{
                            //received some error during receive
                            closeConnection(conn);
                        }
                    }else{
                        //We reacieved complete request, so process it. 
                        if(conn.isRequestReadingDone){
                            struct epoll_event newEvent = events[i];
                            newEvent.events = EPOLLET;
                            int status = conn.setEventForEpollEvent(newEvent);
                            if(status == -1){
                                logger.logError("Unable to remove the the ready to read event(EPOLLIN)");
                                closeConnection(conn);
                                continue;
                            }
                            logger.logDebug("Request headers\n");
                            for(auto pr: conn.req.headers){
                                 logger.logDebug(pr.first + ": " + pr.second);
                            }
                           
                            //add task to the task handler, the worker thread will process the request and sends the response back to client'
                            conn.state = ConnState::PROCESSING;
                            conn.processingStart = Clock::now();
                            conn.timerVersion++;
                            timeoutManager.refresh(conn.clientSocketFd, conn.timerVersion, TimeoutType::PROCESSING);
                            threadPool.addTask(conn, requestHanlderFunction);
                        }
                    }
                }else if(events[i].events & EPOLLOUT){
                    //send the data to the client
                    int sendStatus = sendHttpResponse(conn);
                    if(conn.state == ConnState::PROCESSING){
                        conn.state = ConnState::WRITING_RESPONSE;
                        conn.lastActivity = Clock::now();
                        conn.timerVersion++;
                        timeoutManager.refresh(conn.clientSocketFd, conn.timerVersion, TimeoutType::WRITE);
                    }
                    if(sendStatus == -1 || sendStatus == -2){
                        //close the socket as we got some error during sending data. 
                        //handle closing. 
                        closeConnection(conn);
                    }else{
                        //If send some part or completly with out errors. 
                        //If complete response is sent
                        if(conn.isResponseSendingDone){
                            //handle the closing or keep live thing here.
                            if(conn.isKeepAliveConnection()){
                                conn.state = ConnState::KEEP_ALIVE_WAIT;
                                conn.lastActivity = Clock::now();
                                conn.timerVersion++;
                                timeoutManager.refresh(conn.clientSocketFd, conn.timerVersion, TimeoutType::IDLE);
                                logger.logInfo("Keeping the connection live");
                                conn.clearCurrentRequest();
                                conn.enableEpollin();
                            }else{
                                closeConnection(conn);
                            }
                        }else{
                            //still need to send some more data, when space is available to send in buffers, will try again. 
                            continue;
                        }
                    }
                }
            }
        }

        
    }
}

void Server::handleTimeouts(){
    // Periodically check for expired timers
        auto expiredEntries = timeoutManager.collectExpired();
        for (auto &entry : expiredEntries) {
            auto it = connectionMap.find(entry.fd);
            if (it == connectionMap.end()) continue;
            auto &conn = it->second;

            // Skip stale timers (version mismatch)
            if (conn.timerVersion != entry.version) continue;

            // Handle timeout by type
            cout<<"connection timedout"<<endl;
            switch (entry.type) {
                case TimeoutType::READ:
                    logger.logInfo("Connection timed out (Request timeout), socked fd: " + to_string(entry.fd));
                    break;
                case TimeoutType::WRITE:
                    logger.logInfo("Connection timed out (Response timeout), socked fd: " + to_string(entry.fd));
                    break;
                case TimeoutType::PROCESSING:
                    logger.logInfo("Connection timed out, (Too longer to process), socked fd: " + to_string(entry.fd));
                    break;
                case TimeoutType::IDLE:
                    logger.logInfo("Connection timed out (No activity found), socked fd: " + to_string(entry.fd));
                    break;
            }
            closeConnection(conn);
        }
}

void Server::closeConnection(ClientConnection &conn){
    int clientFd = conn.clientSocketFd;
    conn.deleteFdFromEpoll();
    connectionMap.erase(clientFd);
    close(clientFd);
    logger.logInfo("Connection closed successfully");
}

int Server::sendHttpResponse(ClientConnection &conn){
    while(conn.currentlySentBytes< conn.totalResponseLength){
        ssize_t bytesSent = send(conn.clientSocketFd, conn.responsePtr + conn.currentlySentBytes, conn.totalResponseLength - conn.currentlySentBytes, 0);
        if(bytesSent == -1){
            //interupped, just try again. 
            if(errno == EINTR){
                continue;
            }else if(errno == EAGAIN || errno == EWOULDBLOCK){
                //we are able to send all the data we could, we will try again when some space is avaialbe.
                return 1;
            }else if(errno == EPIPE || errno == ECONNRESET){
                //client closed the connection. 
                logger.logWarn("Connection closed by the client");
                return -2;
            }else{
                //some other error occured. 
                logger.logError("Some error occured while sending data to client:" + string(strerror(errno)));
                return -1;
            }
        }else{
            conn.currentlySentBytes = conn.currentlySentBytes + bytesSent;
        }
    }

    conn.isResponseSendingDone = true;
    return 1;
}

int Server::receiveHttpRequest(ClientConnection &conn){
    string &headerBuffer = conn.requestHeaders;
    string &bodyBuffer = conn.requestBody;
    int clientSocketFd = conn.clientSocketFd;
    char tempBuffer[4096];
    bool &headerReceived = conn.headerReceived;
    size_t &contentLength = conn.contentLength;
    size_t &totalBodyBytesReceived = conn.totalBodyBytesReceived;
    while(true){
        int bytesReceived = recv(clientSocketFd, tempBuffer, sizeof(tempBuffer) , 0);
        //Connection closed or error
        if(bytesReceived == 0){
            //0 indicating connection closed
            //only executes when headers are not yet received completely or body not fully received
            logger.logWarn("Connection closed before complete request was received");
            return 0;
        }else if(bytesReceived < 0){
            //-1 indicating error
            if(errno == EINTR){
                //Interrupted by signal, try again
                continue;
            }else if(errno == EAGAIN || errno == EWOULDBLOCK){
                //all the available data is completed, so return now
                return 1;
            }else{
                logger.logError("Receive error: " + string(strerror(errno)));
                return -1;
            }
        }else{
            if(!headerReceived){
                headerBuffer.append(tempBuffer, bytesReceived);
                size_t headerEndPos = headerBuffer.find("\r\n\r\n");
                if(headerEndPos != string::npos){
                    headerReceived = true;
                    // Find where "Content-Length" starts
                    string lowerCaserHeader = headerBuffer.substr(0, headerEndPos);
                    
                    //handle case insensitivity
                    transform(lowerCaserHeader.begin(), lowerCaserHeader.end(), lowerCaserHeader.begin(), ::tolower);
                    size_t contentPos = lowerCaserHeader.find("content-length:", 0);
                    if (contentPos != string::npos) {
                        // find end of that line (\r\n)
                        size_t contentEnd = lowerCaserHeader.find("\r\n", contentPos);
                        
                        // extract substring between the colon and \r\n
                        string lenStr = lowerCaserHeader.substr(contentPos + 15, contentEnd - (contentPos + 15)); 
                        // 15 = length of "Content-Length:" including colon

                        // trim whitespace (just in case)
                        size_t first = lenStr.find_first_not_of(" \t");
                        size_t last  = lenStr.find_last_not_of(" \t");
                        if (first != std::string::npos)
                            lenStr = lenStr.substr(first, last - first + 1);
                        else
                            lenStr.clear();
                        
                        try{
                            contentLength = stoul(lenStr);
                            if(contentLength <= 0){
                                //No body to receive
                                bodyBuffer = "";
                                conn.isRequestReadingDone = true;
                                return 1;
                            }
                        }catch(exception& e){
                            logger.logWarn("Invalid Content-Length value: " + lenStr);
                            return -2;
                        }
                    }else{
                        // Content-Length header not found, assuming no body
                        bodyBuffer = "";
                        conn.isRequestReadingDone = true;
                        return 1;
                    }

                    // Move any extra data (part of body) to bodyBuffer
                    size_t bodyStartPos = headerEndPos + 4; // 4 is length of "\r\n\r\n"
                    if(headerBuffer.length() > bodyStartPos){
                        bodyBuffer = headerBuffer.substr(bodyStartPos);
                        totalBodyBytesReceived = bodyBuffer.length();
                    }
                    // Keep only headers in headerBuffer. Remove last \r\n\r\n
                    headerBuffer.erase(bodyStartPos); 

                }
            }else{
                bodyBuffer.append(tempBuffer, bytesReceived);
                totalBodyBytesReceived += bytesReceived;
            }
            //check whether we received complete body
            if(totalBodyBytesReceived == contentLength){
                conn.isRequestReadingDone = true;
                return 1;
            }else if(totalBodyBytesReceived > contentLength){
                //Received more than expected
                //Bad request
                return -2;
            }

             //handle size limits. Saves from DoS attacks
            if (headerBuffer.size() > 8192) {
                logger.logWarn("Header too large");
                return -2;
            }

            //For now we only allow body size < ~10MB
            if (contentLength > 10000000) {
                logger.logWarn("Body too large");
                return -2;
            }

            continue; 
        }
    }
   
}

void Server::getIpStr(const struct sockaddr *sa, char *s, size_t maxlen){
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
            break;
    }
}

void Server::listen(int port){
    //Listening for connections on the socket
    int serverSocketFd = createServerSocket(port);
    ::listen(serverSocketFd, BACKLOG);
    logger.logInfo("Server listening on port " + to_string(port));

    //listening and handling incoming requests
    listenRequests(serverSocketFd);
}

int Server::createServerSocket(int port){
    //setting up the address struct, responsible for server address and port
    struct addrinfo address, *addressList;
    memset(&address, 0, sizeof address);

    address.ai_family = AF_UNSPEC;
    address.ai_socktype = SOCK_STREAM;
    address.ai_flags = AI_PASSIVE;

    const char *port_str = to_string(port).c_str();
    int status = getaddrinfo(NULL, port_str, &address, &addressList);
    if (status != 0) {
        logger.logError("Getaddrinfo error: " + string(gai_strerror(status)));
        exit(1);
    }

    //Fetching the first valid address and binding to it
    struct sockaddr *addr = NULL;
    int serverSoc = -1;
    for(struct addrinfo *p = addressList; p!=NULL; p = p->ai_next){
        //Creating server side socket
        //Make this socket a non blocking socket
        //In general, accept() call is blocking meaning, when you call accept and there no client connection available, the thread will get blocked until a client initiates a new connection. 
        //when you make a socket non blocking, then if you make call to accept(), it will accept if a new connection if that exists and return -1 and errno to EAGAIN or EWOULDBLOCK  indicating no active connection request are present, but it will not block, so you can go and do some other task instead of getting blocked like sending or receiving data for some other sockets. 
        if(serverSoc = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol); serverSoc == -1){
            logger.logError("Socket error: " + string(strerror(errno)));
            continue;
        }
        int val = 1;
        if(setsockopt(serverSoc, SOL_SOCKET, SO_REUSEADDR, &val, sizeof (int)) == -1){
            close (serverSoc);
            continue;
        }
        //Binding server side socket to the address
        if(bind(serverSoc, p->ai_addr,p->ai_addrlen) == -1){
            close (serverSoc);
            logger.logError("Bind error: " + string(strerror(errno)));
            continue;
        }else{
            addr = p->ai_addr;
            break;
        }
    }

    //No valid address found
    if(addr == NULL){
        logger.logError("Failed to bind to any address");
        exit(1);
    }
    freeaddrinfo(addressList); //Freeing the address list allocated by getaddrinfo
    return serverSoc;
}

void Server::use(middlewareFunction func){
    middlewareFunctions.push_back(func);
}

void Server::route(const string &method, const string &path, handlerFunction &handler){
    if(find(SUPPORTED_METHODS.begin(), SUPPORTED_METHODS.end(), method) == SUPPORTED_METHODS.end()){
        logger.logWarn("Attempted to register unsupported HTTP method: " + method);
        throw NotSupportedException("HTTP Method " + method + " is not supported");
    }
    
    string routeKey = path;
    routeHandlers.push_back(RouteHandler(method, path, handler)); 
}

handlerFunction Server::getRouteHandlerForPath(Request &request){
    //We basically first match the path and then check if the method matches, we will pick the first match and return
    string method = request.method;
    string path = request.path;
    for(auto &routeHandler: routeHandlers){
        unordered_map<string, string> params;
        bool isPathMatched = validatePath(routeHandler.path, path, params);
        if(isPathMatched && routeHandler.method == method){
            request.params = params;
            return routeHandler.handler;
        }
    }
    //return empty function if no match found
    return handlerFunction(); 
}

//the params in a dynamic route will defined as :paramName. Ex: /user/:id/profile
bool Server::validatePath(string routePath, string requestPath, unordered_map<string, string> &params){
    vector<string> routeSegments;
    vector<string> requestSegments;

    stringstream routeSS(routePath);
    string segment;
    while(getline(routeSS, segment, '/')){
        if(!segment.empty()){
            routeSegments.push_back(segment);
        }
    }

    stringstream requestSS(requestPath);
    while(getline(requestSS, segment, '/')){
        if(!segment.empty()){
            requestSegments.push_back(segment);
        }
    }

    if(routeSegments.size() != requestSegments.size()){
        return false;
    }

    for(size_t i=0; i<routeSegments.size(); i++){
        if(routeSegments[i].front() == ':' && routeSegments[i].length() > 1){
            //parameter segment
            string paramName = routeSegments[i].substr(1);
            params[paramName] = requestSegments[i];
        }else if(routeSegments[i] != requestSegments[i]){
            //no match
            params.clear();
            return false;
        }
    }

    return true;
}

void Server::get(const string &path, handlerFunction handler){
    route("GET", path, handler);
}

void Server::post(const string &path, handlerFunction handler){
    route("POST", path, handler);
}

void Server::put(const string &path, handlerFunction handler){
    route("PUT", path, handler);
}

void Server::del(const string &path, handlerFunction handler){
    route("DELETE", path, handler);
}