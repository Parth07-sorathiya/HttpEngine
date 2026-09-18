#include<threadpool.h>
#include<string.h>
#include<mutex>
#include<condition_variable>
#include<iostream>

using namespace std;
#define DEFAULT_THREAD_COUNT 10

Task::Task(ClientConnection &conn, requestHandler& handlerFunction):conn(conn), handlerFunction(handlerFunction){}

ThreadPool::ThreadPool(int threads){
    threadsCount = threads;
    for(int i=0; i<threadsCount; i++){
        workers.emplace_back([this](){
            while(true){
                unique_lock<mutex> ul(lock);
                cv.wait(ul, [this](){return stop || !taskQueue.empty();});
                if(stop)break;
                Task currTask = std::move(taskQueue.front());
                taskQueue.pop();
                ul.unlock();
                currTask.handlerFunction(currTask.conn);
            }
        });
    }
}

ThreadPool::ThreadPool(){
    threadsCount = DEFAULT_THREAD_COUNT;
}

void ThreadPool::addTask(ClientConnection &conn, requestHandler &handlerFunction){
    {
        unique_lock<mutex> ul(lock);
        taskQueue.push(Task(conn, handlerFunction));
    }
    cv.notify_one();
}

ThreadPool::~ThreadPool(){
    {
        std::unique_lock<std::mutex> ul(lock);
        stop = true;
    }
    cv.notify_all();
    for(int i=0; i<threadsCount; i++){
        workers[i].join();
    }
}