#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "TelemetryManager.h"
#include "TelemetryPacket.h"
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <imgui.h>
#include <random>
#include <chrono>

// Engine headers (C linkage)
#include "common_structs.h"
#include "defines.h"

// Forward declare C globals with explicit C linkage to avoid mangling issues.
extern "C" {
    extern Player gPlayers[];
    extern s32 gGamestate;
    extern f32 gDeltaTime;
    extern f32 gCourseTimer;
    extern f32 gTimePlayerLastTouchedFinishLine[];
}

#pragma comment(lib, "Ws2_32.lib")

struct TelemetryManager::Impl {
    SOCKET socket = INVALID_SOCKET;
    sockaddr_in destAddr;
    uint64_t instanceId = 0;
    uint64_t sessionId = 0;
    uint64_t packetCounter = 0;
    double sessionStartTime = 0;

    float lastCourseTimer = 0;
    int16_t lastLakituProps = 0;
    float lastPos[3] = { 0, 0, 0 };
    uint32_t discontinuityCounter = 0;
};

TelemetryManager* TelemetryManager::mInstance = nullptr;

TelemetryManager* TelemetryManager::GetInstance() {
    if (mInstance == nullptr) {
        mInstance = new TelemetryManager();
    }
    return mInstance;
}

TelemetryManager::TelemetryManager() {
    pImpl = new Impl();
    
    std::random_device rd;
    std::mt19937_64 gen(rd());
    pImpl->instanceId = gen();
}

TelemetryManager::~TelemetryManager() {
    if (pImpl->socket != INVALID_SOCKET) {
        closesocket(pImpl->socket);
    }
    delete pImpl;
    WSACleanup();
}

void TelemetryManager::Init() {
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        SPDLOG_ERROR("TelemetryManager: WSAStartup failed: {}", res);
        return;
    }

    pImpl->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (pImpl->socket == INVALID_SOCKET) {
        SPDLOG_ERROR("TelemetryManager: Socket creation failed: {}", WSAGetLastError());
        return;
    }

    LoadSettings();
    SPDLOG_INFO("TelemetryManager: Initialized");
}

void TelemetryManager::LoadSettings() {
    mIsEnabled = CVarGetInteger("gTelemetry.Enabled", 0);
    mSpeedFactor = CVarGetFloat("gTelemetry.SpeedFactor", 3.6f);
    std::string ip = CVarGetString("gTelemetry.IP", "127.0.0.1");
    int port = CVarGetInteger("gTelemetry.Port", 20777);

    pImpl->destAddr.sin_family = AF_INET;
    pImpl->destAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &pImpl->destAddr.sin_addr);
}

void TelemetryManager::Update() {
    if (!mIsEnabled) return;
    if (gGamestate != RACING && gGamestate != ENDING) {
        pImpl->sessionId = 0; // Reset session when not racing
        return;
    }
    if (pImpl->socket == INVALID_SOCKET) return;

    if (pImpl->sessionId == 0) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        pImpl->sessionId = gen();
        pImpl->sessionStartTime = gCourseTimer;
    }

    Player* p = &gPlayers[0];
    static float prevLocalVel[3] = { 0, 0, 0 };

    // Detect discontinuities (teleports, resets, Lakitu)
    bool discontinuity = false;

    // 1. Race Restart / Reset
    if (gCourseTimer < pImpl->lastCourseTimer) {
        discontinuity = true;
    }

    // 2. Lakitu Drop (HELD_BY_LAKITU is 0x2)
    if ((pImpl->lastLakituProps & 0x02) && !(p->lakituProps & 0x02)) {
        discontinuity = true;
    }

    // 3. Large Position Jump (Threshold: 500 units)
    float dx = p->pos[0] - pImpl->lastPos[0];
    float dy = p->pos[1] - pImpl->lastPos[1];
    float dz = p->pos[2] - pImpl->lastPos[2];
    float distSq = dx * dx + dy * dy + dz * dz;
    if (distSq > (500.0f * 500.0f)) {
        discontinuity = true;
    }

    if (discontinuity) {
        pImpl->discontinuityCounter++;
    }

    // Update tracking for next frame
    pImpl->lastCourseTimer = gCourseTimer;
    pImpl->lastLakituProps = p->lakituProps;
    pImpl->lastPos[0] = p->pos[0];
    pImpl->lastPos[1] = p->pos[1];
    pImpl->lastPos[2] = p->pos[2];

    TelemetryPacket packet{};
    packet.GameSignature = TELEMETRY_GAME_SIGNATURE;
    packet.TelemetrySignature = TELEMETRY_PROTOCOL_SIGNATURE;
    packet.LayoutMajorVersion = 1;
    packet.LayoutMinorVersion = 0;
    
    packet.EmitterInstanceId = pImpl->instanceId;
    packet.PacketId = 0;
    packet.PacketsCounter = ++pImpl->packetCounter;
    packet.IsSessionRunning = 1;
    packet.IsSessionPaused = 0;
    packet.SessionId = pImpl->sessionId;
    packet.IsReplay = 0;
    packet.IsUserInControl = 1;
    packet.IsAIInControl = 0;
    packet.IsSpectator = 0;
    packet.SessionTimeSeconds = gCourseTimer;
    packet.PhysicsDiscontinuityCounter = pImpl->discontinuityCounter;

    // Orientation (s16 binangles to Degrees)
    // 0-65535 maps to 0-360
    auto binToDeg = [](s16 angle) {
        return (float)angle * (360.0f / 65536.0f);
    };

    packet.PitchDegrees = binToDeg(p->rotation[0]);
    packet.YawDegrees   = binToDeg(p->rotation[1]);
    packet.RollDegrees  = binToDeg(p->rotation[2]);

    // Local Velocity calculation (World Velocity -> Local Space)
    // LocalX = worldVel dot orientationMatrix[0]
    // LocalY = worldVel dot orientationMatrix[1]
    // LocalZ = worldVel dot orientationMatrix[2]
    float localVelX = p->velocity[0] * p->orientationMatrix[0][0] + p->velocity[1] * p->orientationMatrix[0][1] + p->velocity[2] * p->orientationMatrix[0][2];
    float localVelY = p->velocity[0] * p->orientationMatrix[1][0] + p->velocity[1] * p->orientationMatrix[1][1] + p->velocity[2] * p->orientationMatrix[1][2];
    float localVelZ = p->velocity[0] * p->orientationMatrix[2][0] + p->velocity[1] * p->orientationMatrix[2][1] + p->velocity[2] * p->orientationMatrix[2][2];

    packet.LocalVelocityLateralMps = localVelX;
    packet.LocalVelocityUpMps = localVelY;
    packet.LocalVelocityForwardMps = localVelZ;

    // Local Acceleration (G-forces)
    float dt = gDeltaTime > 0 ? gDeltaTime : (1.0f / 60.0f);
    packet.LocalSwayMs2 = (localVelX - prevLocalVel[0]) / dt;
    packet.LocalHeaveMs2 = (localVelY - prevLocalVel[1]) / dt;
    packet.LocalSurgeMs2 = (localVelZ - prevLocalVel[2]) / dt;

    prevLocalVel[0] = localVelX;
    prevLocalVel[1] = localVelY;
    prevLocalVel[2] = localVelZ;

    // Position (Double)
    packet.VehiclePositionEast = (double)p->pos[0];
    packet.VehiclePositionUp = (double)p->pos[1];
    packet.VehiclePositionNorth = (double)-p->pos[2]; // N64 Z is South

    // Race State
    packet.GroundSpeedKmh = p->speed * mSpeedFactor;
    packet.CompletedLaps = (uint32_t)p->lapCount;
    packet.RacePosition = p->currentRank;
    
    if (gGamestate == RACING) {
        packet.CurrentLapTime = std::max(0.0, (double)gCourseTimer - gTimePlayerLastTouchedFinishLine[0]);
    } else {
        packet.CurrentLapTime = 0;
    }
    
    packet.IsRaceActive = (gGamestate == RACING);

    // Events
    packet.EventFlags = 0;
    if (p->hopFrameCounter > 0) packet.EventFlags |= TELEMETRY_EVENT_JUMPING;
    if (p->boostTimer > 0) packet.EventFlags |= TELEMETRY_EVENT_BOOSTING;
    if (p->effects & 0x4000) packet.EventFlags |= TELEMETRY_EVENT_HIT;
    if (p->effects & 0x10000000) packet.EventFlags |= TELEMETRY_EVENT_SPINOUT;

    sendto(pImpl->socket, (const char*)&packet, sizeof(packet), 0, (sockaddr*)&pImpl->destAddr, sizeof(pImpl->destAddr));
}

void TelemetryManager::DrawSettings() {
    bool enabled = (bool)CVarGetInteger("gTelemetry.Enabled", 0);
    if (ImGui::Checkbox("Enable SimHub Telemetry", &enabled)) {
        CVarSetInteger("gTelemetry.Enabled", enabled);
        LoadSettings();
        Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }

    if (enabled) {
        char ipBuf[64];
        std::string ip = CVarGetString("gTelemetry.IP", "127.0.0.1");
        strncpy(ipBuf, ip.c_str(), sizeof(ipBuf) - 1);
        ipBuf[sizeof(ipBuf) - 1] = '\0';
        if (ImGui::InputText("SimHub IP", ipBuf, sizeof(ipBuf))) {
            CVarSetString("gTelemetry.IP", ipBuf);
            LoadSettings();
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }

        int port = CVarGetInteger("gTelemetry.Port", 20777);
        if (ImGui::InputInt("SimHub Port", &port)) {
            CVarSetInteger("gTelemetry.Port", port);
            LoadSettings();
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        
        ImGui::TextDisabled("Default Port: 20777");

        if (ImGui::InputFloat("Speed Scaling Factor", &mSpeedFactor, 0.1f, 1.0f, "%.2f")) {
            CVarSetFloat("gTelemetry.SpeedFactor", mSpeedFactor);
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        ImGui::TextDisabled("MK64 internal speed * this factor = GroundSpeedKmh");
    }
}

extern "C" {
void TelemetryManager_Init() {
    TelemetryManager::GetInstance()->Init();
}

void TelemetryManager_Update() {
    TelemetryManager::GetInstance()->Update();
}

void TelemetryManager_DrawSettings() {
    TelemetryManager::GetInstance()->DrawSettings();
}
}
