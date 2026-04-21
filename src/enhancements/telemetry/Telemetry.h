#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <SDL2/SDL_net.h>
#include <thread>
#include <atomic>
#include <mutex>
#include "PacketPC2.h"

class Telemetry {
public:
    static Telemetry* Instance;

    static void Init();
    static void Tick(float dt);

    void SendPacket(const sTelemetryData& packet);
    sTelemetryData GetPacket();

private:
    Telemetry();
    ~Telemetry();

    void WorkerLoop();

    UDPsocket mSocket;
    IPaddress mIp;
    std::thread mWorker;
    std::atomic<bool> mRunning;

    sTelemetryData mCurrentPacket;
    std::mutex mPacketMutex;
    float mSavedVelocity[3];
};

#endif // TELEMETRY_H
