#pragma once
#include<request.h>
#include<response.h>
#include<functional>
#include<queue>
#include<thread>
#include<mutex>
#include<condition_variable>

using requestHandler =  std::function<void(ClientConnection&)>;

class Task{
    public:
    Task(ClientConnection &, requestHandler&handlerFunction);
    Task(); 
    ClientConnection &conn;
    requestHandler &handlerFunction;
};

class ThreadPool{
    private:
        int threadsCount;
        std::queue<Task> taskQueue;
        std::vector<std::thread> workers;
        std::mutex lock;
        std::condition_variable cv;
        bool stop = false;

    public:
    ThreadPool(int threads);
    ThreadPool();
    void addTask(ClientConnection &conn, requestHandler &handlerFunction);
    ~ThreadPool();
};
