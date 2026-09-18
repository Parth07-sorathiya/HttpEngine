#pragma once
#include <chrono>
#include <queue>
#include <vector>

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

struct TimeoutConfig {
    int readTimeoutSec = 10;
    int writeTimeoutSec = 10;
    int processingTimeoutSec = 20;
    int idleTimeoutSec = 30;
};

enum class TimeoutType {
    READ,
    WRITE,
    PROCESSING,
    IDLE
};

struct TimeoutEntry {
    TimePoint expiresAt;
    int fd;
    uint64_t version;
    TimeoutType type;
    bool operator>(const TimeoutEntry& other) const {
        return expiresAt > other.expiresAt;
    }
};

class TimeoutManager {
public:
    TimeoutManager(TimeoutConfig cfg);
    void refresh(int fd, uint64_t version, TimeoutType type);
    std::vector<TimeoutEntry> collectExpired();
private:
    TimeoutConfig config;
    std::priority_queue<TimeoutEntry, std::vector<TimeoutEntry>, std::greater<TimeoutEntry>> heap;
    void print_age(TimePoint tp);
};