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
#include<server.h>
#include<middleware.h>
#include<routeHandler.h>
#include<request.h>
#include<response.h>
#include<math.h>
using namespace std;

// ─────────────────────────── Middleware Examples ───────────────────────────
middlewareFunction authMiddleware = [](Request &req, Response &res) -> void {
    // Simple mock check for ?token=123
    if (req.queryParams.count("token") == 0 || req.queryParams["token"] != "123") {
        res.setStatus(401);
        res.setBody("Unauthorized\n");
        res.sendResponse();
    }
};

//Both middleware and handler functions will be able to access query params, dynamic path params and headers etc 
//Everything is abstracted away
//The user can simply focus on writing core logic
middlewareFunction logMiddleware = [](Request &req, Response &res)->void{
    cout << "[Middleware] " << req.method << " " << req.path << endl;
    cout<<"Printing the headers"<<endl;
    for(auto header: req.headers){
        cout<<"Header req["<<header.first<<"]"<<"="<<header.second<<endl;
    }

    cout<<"Printing the query params"<<endl;
    for(auto qParam: req.queryParams){
        cout<<"Query param: "<<qParam.first<<"="<<qParam.second<<endl;
    }

    cout<<"Printing the dynamic path params"<< endl;
     for(auto param: req.params){
        cout<<"path param: "<<param.first<<"="<<param.second<<endl;
    }

    cout<<"Printing the body of the request"<<endl;
    cout<<req.body<<endl;
   
};

// ─────────────────────────── Route Handlers ───────────────────────────
handlerFunction homeHandler = [](Request &req, Response &res) {
    res.setStatus(200);
    res.setBody("Welcome to root!\n");
    res.sendResponse();
};

handlerFunction echoHandler = [](Request &req, Response &res) {
    // Example: dynamic params + query parsing
    string user = req.params.count("user") ? req.params.at("user") : "unknown";
    string q = req.queryParams.count("q") ? req.queryParams.at("q") : "";
    string msg = "Hello, " + user + "! Query: " + q + "\n";
    res.setStatus(200);
    res.setBody(msg);
    res.sendResponse();
};

handlerFunction cpuHeavyHandler = [](Request &req, Response &res) {
    double x = 0;
    for (int i = 0; i < 1000000; ++i) x += std::sin(i);  // CPU busy loop
    res.setStatus(200);
    res.setBody("CPU work done\n");
    res.sendResponse();
};

handlerFunction ioHeavyHandler = [](Request &req, Response &res) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Simulated DB/IO wait
    res.setStatus(200);
    res.setBody("IO operation complete\n");
    res.sendResponse();
};

handlerFunction jsonHandler = [](Request &req, Response &res) {
    string body = R"({"status": "ok", "message": "JSON example"})";
    res.setStatus(200);
    res.setHeader("Content-Type", "application/json");
    res.setBody(body);
    res.sendResponse();
};

handlerFunction errorHandler = [](Request &req, Response &res) {
    res.setStatus(500);
    res.setBody("Something went wrong internally\n");
    res.sendResponse();
};

// ─────────────────────────── Main ───────────────────────────
int main() {
    int port = 9000;
    //Here threads in workerpool will get blocked on io, so need to have more threads so that some will be processing cpu bound tasks instead of all getting blocked. 
    int threadPoolThreads = 512;

    cout << "Starting HelixHTTP on port " << port
         << " with " << threadPoolThreads << " worker threads..." << endl;

    Server server(threadPoolThreads);

    // Global middleware
    server.use(logMiddleware);
    // server.use(authMiddleware); // Uncomment to enforce token check globally

    // Routes
    server.get("/", homeHandler);
    server.get("/cpu", cpuHeavyHandler);
    server.get("/io", ioHeavyHandler);
    server.get("/json", jsonHandler);
    server.get("/user/:user", echoHandler);  // Example: dynamic param
    server.get("/error", errorHandler);

    server.listen(port);
    return 0;
}