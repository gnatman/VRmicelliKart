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
#include <algorithm>
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

    // Acceleration filter state
    float prevLocalVel[3] = { 0, 0, 0 };
    float smoothedSurge = 0;
    float smoothedSway = 0;
    float smoothedHeave = 0;
    bool accelInitialized = false;
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
    mSmoothingAlpha = CVarGetFloat("gTelemetry.SmoothingAlpha", 0.3f);
    mMaxAccel = CVarGetFloat("gTelemetry.MaxAccel", 30.0f);
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
        pImpl->accelInitialized = false;
        return;
    }
    if (pImpl->socket == INVALID_SOCKET) return;

    if (pImpl->sessionId == 0) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        pImpl->sessionId = gen();
        pImpl->sessionStartTime = gCourseTimer;
        pImpl->accelInitialized = false;
    }

    Player* p = &gPlayers[0];

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
    float localVelX = p->velocity[0] * p->orientationMatrix[0][0] + p->velocity[1] * p->orientationMatrix[0][1] + p->velocity[2] * p->orientationMatrix[0][2];
    float localVelY = p->velocity[0] * p->orientationMatrix[1][0] + p->velocity[1] * p->orientationMatrix[1][1] + p->velocity[2] * p->orientationMatrix[1][2];
    float localVelZ = p->velocity[0] * p->orientationMatrix[2][0] + p->velocity[1] * p->orientationMatrix[2][1] + p->velocity[2] * p->orientationMatrix[2][2];

    // Convert from game units to m/s using the calibrated speed factor.
    // If speed * mSpeedFactor = km/h, then velocity * (mSpeedFactor / 3.6) = m/s.
    float velScale = mSpeedFactor / 3.6f;
    float lvX = localVelX * velScale;
    float lvY = localVelY * velScale;
    float lvZ = localVelZ * velScale;

    packet.LocalVelocityLateralMps = lvX;
    packet.LocalVelocityUpMps = lvY;
    packet.LocalVelocityForwardMps = lvZ;

    // Local Acceleration with EMA filtering, clamping, and discontinuity suppression
    float dt = gDeltaTime > 0 ? gDeltaTime : (1.0f / 60.0f);

    if (discontinuity || !pImpl->accelInitialized) {
        // Suppress acceleration spikes during teleports, Lakitu, session start
        pImpl->prevLocalVel[0] = lvX;
        pImpl->prevLocalVel[1] = lvY;
        pImpl->prevLocalVel[2] = lvZ;
        pImpl->smoothedSurge = 0;
        pImpl->smoothedSway = 0;
        pImpl->smoothedHeave = 0;
        pImpl->accelInitialized = true;

        packet.LocalSurgeMs2 = 0;
        packet.LocalSwayMs2 = 0;
        packet.LocalHeaveMs2 = 0;
    } else {
        // Raw acceleration (velocity derivative)
        float rawSurge = (lvZ - pImpl->prevLocalVel[2]) / dt;
        float rawSway  = (lvX - pImpl->prevLocalVel[0]) / dt;
        float rawHeave = (lvY - pImpl->prevLocalVel[1]) / dt;

        // Exponential Moving Average low-pass filter
        float a = mSmoothingAlpha;
        pImpl->smoothedSurge = a * rawSurge + (1.0f - a) * pImpl->smoothedSurge;
        pImpl->smoothedSway  = a * rawSway  + (1.0f - a) * pImpl->smoothedSway;
        pImpl->smoothedHeave = a * rawHeave + (1.0f - a) * pImpl->smoothedHeave;

        // Hard clamp to prevent exceeding platform travel limits
        float maxA = mMaxAccel;
        packet.LocalSurgeMs2 = std::clamp(pImpl->smoothedSurge, -maxA, maxA);
        packet.LocalSwayMs2  = std::clamp(pImpl->smoothedSway,  -maxA, maxA);
        packet.LocalHeaveMs2 = std::clamp(pImpl->smoothedHeave, -maxA, maxA);

        pImpl->prevLocalVel[0] = lvX;
        pImpl->prevLocalVel[1] = lvY;
        pImpl->prevLocalVel[2] = lvZ;
    }

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

    bool isHit = ((p->effects & (0x400 | 0x4000 | 0x01000000 | HIT_BY_ITEM_EFFECT | LIGHTNING_EFFECT)) != 0) ||
                 ((p->kartGraphics & CRASH) != 0);
    if (isHit) packet.EventFlags |= TELEMETRY_EVENT_HIT;

    bool isSpinout = ((p->effects & (0x40 | 0x80 | 0x20000 | 0x10000000)) != 0) ||
                     ((p->kartProps & DRIVING_SPINOUT) != 0) ||
                     ((p->kartGraphics & WHIRRR) != 0) ||
                     ((p->triggers & (START_SPINOUT_TRIGGER | SPINOUT_TRIGGER | DRIVING_SPINOUT_TRIGGER | HIT_BANANA_TRIGGER)) != 0);
    if (isSpinout) packet.EventFlags |= TELEMETRY_EVENT_SPINOUT;

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
        ImGui::TextDisabled("Also scales velocity/acceleration to real-world units.");

        ImGui::Separator();
        ImGui::Text("Motion Filtering");

        if (ImGui::SliderFloat("Smoothing (Alpha)", &mSmoothingAlpha, 0.05f, 1.0f, "%.2f")) {
            CVarSetFloat("gTelemetry.SmoothingAlpha", mSmoothingAlpha);
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        ImGui::TextDisabled("Lower = smoother (more lag). Higher = responsive (spikier).");

        if (ImGui::InputFloat("Max Acceleration (m/s^2)", &mMaxAccel, 1.0f, 5.0f, "%.1f")) {
            if (mMaxAccel < 1.0f) mMaxAccel = 1.0f;
            CVarSetFloat("gTelemetry.MaxAccel", mMaxAccel);
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        ImGui::TextDisabled("Clamp acceleration to prevent hitting platform limits. ~30 = ~3G.");

        ImGui::Separator();
        ImGui::Text("Live Telemetry Status:");
        if (gGamestate == RACING) {
            Player* p = &gPlayers[0];
            bool isSpin = ((p->effects & (0x40 | 0x80 | 0x20000 | 0x10000000)) != 0) ||
                          ((p->kartProps & DRIVING_SPINOUT) != 0) ||
                          ((p->kartGraphics & WHIRRR) != 0) ||
                          ((p->triggers & (START_SPINOUT_TRIGGER | SPINOUT_TRIGGER | DRIVING_SPINOUT_TRIGGER | HIT_BANANA_TRIGGER)) != 0);
            ImGui::Text("Spinout Active (Bit 3): %s", isSpin ? "YES" : "No");
            ImGui::Text("Yaw: %.1f deg", (float)p->rotation[1] * (360.0f / 65536.0f));
            ImGui::Text("Ground Speed: %.1f km/h", p->speed * mSpeedFactor);
            ImGui::Text("Effects: 0x%08X", p->effects);
        } else {
            ImGui::TextDisabled("Status: Not in active race");
        }
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
