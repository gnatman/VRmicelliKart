#include "Telemetry.h"
#include <chrono>
#pragma comment(lib, "ws2_32.lib")

Telemetry& Telemetry::GetInstance() {
    static Telemetry instance;
    return instance;
}

void Telemetry::Init() {
    mRunning = true;
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    mSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    // Default loopback 5606 for PC2
    mDestAddr.sin_family = AF_INET;
    mDestAddr.sin_port = htons(5606);
    mDestAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
#endif
    mWorker = std::thread(&Telemetry::WorkerThread, this);
}

void Telemetry::Shutdown() {
    mRunning = false;
    if (mWorker.joinable()) {
        mWorker.join();
    }
#ifdef _WIN32
    closesocket(mSocket);
    WSACleanup();
#endif
}

void Telemetry::PushData(const sTelemetryData& data) {
    size_t head = mHead.load(std::memory_order_relaxed);
    size_t nextHead = (head + 1) % RING_SIZE;
    if (nextHead != mTail.load(std::memory_order_acquire)) {
        mRingBuffer[head] = data;
        mHead.store(nextHead, std::memory_order_release);
    }
}

void Telemetry::WorkerThread() {
    while (mRunning) {
        size_t tail = mTail.load(std::memory_order_relaxed);
        if (tail != mHead.load(std::memory_order_acquire)) {
            const sTelemetryData& data = mRingBuffer[tail];
#ifdef _WIN32
            sendto(mSocket, (const char*)&data, sizeof(sTelemetryData), 0, 
                  (sockaddr*)&mDestAddr, sizeof(mDestAddr));
#endif
            mTail.store((tail + 1) % RING_SIZE, std::memory_order_release);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}