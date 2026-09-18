#pragma once
#include<request.h>
#include<response.h>
#include<functional>

using  handlerFunction = std::function<void(Request&, Response&)>;
// using middlewareFunction = std::function<void(Request&, Response&, std::function<void()>)>;

class RouteHandler{
    public:
        std::string method;
        std::string path;
        handlerFunction handler;
        RouteHandler(const std::string &method, const std::string &path, handlerFunction handler)
            : method(method), path(path), handler(handler) {}
};