#include "Telemetry.h"
#include <libultraship.h>
#include <cstring>
#include <chrono>
#include <cmath>

extern "C" {
#include <common_structs.h>
#include <defines.h>
}

extern "C" Player gPlayers[];

Telemetry* Telemetry::Instance = nullptr;

void Telemetry::Init() {
    if (Instance == nullptr) {
        Instance = new Telemetry();
    }
}

Telemetry::Telemetry() : mSocket(NULL), mRunning(false) {
    // Note: SDLNet_Init should only be called once. 
    // If other parts of the game use it, we might need a more global init.
    if (SDLNet_Init() < 0) {
        return;
    }

    const char* host = CVarGetString("gTelemetryHost", "127.0.0.1");
    uint16_t port = (uint16_t)CVarGetInteger("gTelemetryPort", 5606);

    if (SDLNet_ResolveHost(&mIp, host, port) < 0) {
        return;
    }

    // Open a UDP socket on a random port
    mSocket = SDLNet_UDP_Open(0);
    if (!mSocket) {
        return;
    }

    // Initialize packet with default PC2 values
    memset(&mCurrentPacket, 0, sizeof(sTelemetryData));
    mCurrentPacket.base.mPacketType = 0;    // Telemetry
    mCurrentPacket.base.mPacketVersion = 2; // PC2 v2

    memset(mSavedVelocity, 0, sizeof(mSavedVelocity));

    mRunning = true;
    mWorker = std::thread(&Telemetry::WorkerLoop, this);
}

Telemetry::~Telemetry() {
    mRunning = false;
    if (mWorker.joinable()) {
        mWorker.join();
    }
    if (mSocket) {
        SDLNet_UDP_Close(mSocket);
        mSocket = NULL;
    }
    SDLNet_Quit();
}

void Telemetry::WorkerLoop() {
    UDPpacket* packet = SDLNet_AllocPacket(sizeof(sTelemetryData));
    if (!packet) return;

    packet->address = mIp;

    while (mRunning) {
        bool enabled = CVarGetInteger("gTelemetryEnabled", 0);
        if (enabled && mSocket) {
            {
                std::lock_guard<std::mutex> lock(mPacketMutex);
                memcpy(packet->data, &mCurrentPacket, sizeof(sTelemetryData));
                
                // Increment packet numbers
                mCurrentPacket.base.mPacketNumber++;
                mCurrentPacket.base.mCategoryPacketNumber++;
            }
            packet->len = sizeof(sTelemetryData);
            SDLNet_UDP_Send(mSocket, -1, packet);
        }

        // Target rate: ~60Hz (16ms)
        // This prevents 100% CPU usage and matches SimHub's typical expectation.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    SDLNet_FreePacket(packet);
}

void Telemetry::Tick(float dt) {
    if (Instance == nullptr) return;
    if (!CVarGetInteger("gTelemetryEnabled", 0)) return;
    
    // Safety check for dt to avoid division by zero (Rule 2 / T-01-03)
    if (dt <= 0.0f) dt = 0.01666f;

    Player* p = &gPlayers[0];
    
    // Check if player exists in the current game state
    if (!(p->type & PLAYER_EXISTS)) return;

    std::lock_guard<std::mutex> lock(Instance->mPacketMutex);

    // Position (MK64 units -> Meters)
    // Scale 0.01f is a guestimate: 100 units = 1 meter
    Instance->mCurrentPacket.mWorldPosition[0] = p->pos[0] * 0.01f;
    Instance->mCurrentPacket.mWorldPosition[1] = p->pos[1] * 0.01f;
    Instance->mCurrentPacket.mWorldPosition[2] = p->pos[2] * 0.01f;

    // Local Velocity (MK64 units/frame -> m/s)
    // TRACK_TIMER_ITER is 1/60, so we multiply by 60.
    float velX = p->velocity[0] * 60.0f * 0.01f;
    float velY = p->velocity[1] * 60.0f * 0.01f;
    float velZ = p->velocity[2] * 60.0f * 0.01f;
    
    Instance->mCurrentPacket.mLocalVelocity[0] = velX;
    Instance->mCurrentPacket.mLocalVelocity[1] = velY;
    Instance->mCurrentPacket.mLocalVelocity[2] = velZ;

    // Local Acceleration (m/s^2)
    float accelScale = CVarGetFloat("gTelemetryAccelScale", 1.0f);
    Instance->mCurrentPacket.mLocalAcceleration[0] = ((velX - Instance->mSavedVelocity[0]) / dt) * accelScale;
    Instance->mCurrentPacket.mLocalAcceleration[1] = ((velY - Instance->mSavedVelocity[1]) / dt) * accelScale;
    Instance->mCurrentPacket.mLocalAcceleration[2] = ((velZ - Instance->mSavedVelocity[2]) / dt) * accelScale;
    
    Instance->mSavedVelocity[0] = velX;
    Instance->mSavedVelocity[1] = velY;
    Instance->mSavedVelocity[2] = velZ;

    // Orientation (MK64 s16 [0, 65535] -> Radians)
    // p->rotation[1] is Yaw.
    Instance->mCurrentPacket.mOrientation[0] = (float)p->rotation[1] * (3.14159265f / 32768.0f); // Yaw
    Instance->mCurrentPacket.mOrientation[1] = 0; // Pitch
    Instance->mCurrentPacket.mOrientation[2] = 0; // Roll

    // Speed (m/s)
    float speedScale = CVarGetFloat("gTelemetrySpeedScale", 1.0f);
    Instance->mCurrentPacket.mSpeed = p->speed * 60.0f * 0.01f * speedScale;

    // Task 3: Fake RPM and Gear Logic
    // Synthesize data since MK64 lacks these concepts.
    float speedKph = std::abs(Instance->mCurrentPacket.mSpeed * 3.6f);
    
    // Simple 5-gear logic: each gear covers 30 km/h
    int gear = (int)(speedKph / 30.0f) + 1;
    if (gear > 5) gear = 5;
    if (gear < 1) gear = 1;
    
    float gearBaseSpeedKph = (gear - 1) * 30.0f;
    float gearRelativeSpeedKph = speedKph - gearBaseSpeedKph;
    
    // RPM: 1000 (Idle) to 7000, scales with speed within the gear
    float rpm = 1000.0f + (gearRelativeSpeedKph / 30.0f) * 6000.0f;
    
    Instance->mCurrentPacket.mRpm = (uint16_t)rpm;
    Instance->mCurrentPacket.mMaxRpm = 8000;
    
    // mGearNumGears: bits 0-3 = gear, bits 4-7 = num gears
    // PC2: 0=N, 1=1, 2=2... (simplified)
    Instance->mCurrentPacket.mGearNumGears = (uint8_t)(gear & 0x0F) | (5 << 4);
}

sTelemetryData Telemetry::GetPacket() {
    std::lock_guard<std::mutex> lock(mPacketMutex);
    return mCurrentPacket;
}

void Telemetry::SendPacket(const sTelemetryData& packet) {
    std::lock_guard<std::mutex> lock(mPacketMutex);
    // Keep the header info but update the rest
    uint32_t pNum = mCurrentPacket.base.mPacketNumber;
    uint32_t cNum = mCurrentPacket.base.mCategoryPacketNumber;
    
    mCurrentPacket = packet;
    
    mCurrentPacket.base.mPacketNumber = pNum;
    mCurrentPacket.base.mCategoryPacketNumber = cNum;
    mCurrentPacket.base.mPacketType = 0;
    mCurrentPacket.base.mPacketVersion = 2;
}
