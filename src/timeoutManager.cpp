#include <chrono>
#include <queue>
#include <vector>
#include <mutex>
#include <unordered_map>
#include<timeoutManager.h>
#include <iostream>
using namespace std;

TimeoutManager::TimeoutManager(TimeoutConfig cfg) : config(cfg) {}

void TimeoutManager::print_age(TimePoint tp) {
    using namespace std::chrono;
    auto age = duration_cast<milliseconds>(Clock::now() - tp).count();
    std::cout << "age: " << age << " ms\n";
}

void TimeoutManager::refresh(int fd, uint64_t version, TimeoutType type) {
    TimePoint expires;
    switch (type) {
        case TimeoutType::READ:
            expires = Clock::now() + chrono::seconds(config.readTimeoutSec);
            break;
        case TimeoutType::WRITE:
            expires = Clock::now() + chrono::seconds(config.writeTimeoutSec);
            break;
        case TimeoutType::PROCESSING:
            expires = Clock::now() + chrono::seconds(config.processingTimeoutSec);
            break;
        case TimeoutType::IDLE:
            expires = Clock::now() + chrono::seconds(config.idleTimeoutSec);
            break;
    }
}

vector<TimeoutEntry> TimeoutManager::collectExpired() {
    vector<TimeoutEntry> expired;
    TimePoint now = Clock::now();

    while (!heap.empty() && heap.top().expiresAt <= now) {
        expired.push_back(heap.top());
        heap.pop();
    }
    return expired;
}