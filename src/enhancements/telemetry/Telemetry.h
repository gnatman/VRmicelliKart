#pragma once
#include "PacketPC2.h"
#include <thread>
#include <atomic>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#endif

class Telemetry {
public:
    static Telemetry& GetInstance();
    void Init();
    void Shutdown();
    void PushData(const sTelemetryData& data);

private:
    Telemetry() = default;
    ~Telemetry() = default;
    
    void WorkerThread();
    
    std::thread mWorker;
    std::atomic<bool> mRunning{false};
    
#ifdef _WIN32
    SOCKET mSocket;
    sockaddr_in mDestAddr;
#endif

    // Lockless SPSC ring buffer
    static const int RING_SIZE = 8;
    sTelemetryData mRingBuffer[RING_SIZE];
    std::atomic<size_t> mHead{0};
    std::atomic<size_t> mTail{0};
};