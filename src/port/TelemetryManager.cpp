#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "TelemetryManager.h"
#include "TelemetryPacket.h"
#include "WheelManager.h"
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <random>
#include <chrono>

// Engine headers (C linkage)
#include "common_structs.h"
#include "defines.h"
#include "mk64.h"

// Forward declare C globals with explicit C linkage to avoid mangling issues.
extern "C" {
    extern Player gPlayers[];
    extern s32 gGamestate;
    extern s32 gRaceState;
    extern u16 gDemoMode;
    extern f32 gDeltaTime;
    extern f32 gCourseTimer;
    extern f32 gTimePlayerLastTouchedFinishLine[];
    extern OSContPad gControllerPads[];
}

#pragma comment(lib, "Ws2_32.lib")

struct TelemetryManager::Impl {
    SOCKET socket = INVALID_SOCKET;
    sockaddr_in destAddr;
    uint64_t instanceId = 0;
    uint64_t sessionId = 0;
    uint64_t packetCounter = 0;
    double sessionStartTime = 0;

    float lastCourseTimer = 0.0f;
    int16_t lastLakituProps = 0;
    float lastPos[3] = { 0.0f, 0.0f, 0.0f };
    uint32_t discontinuityCounter = 0;

    // Angular rate tracking
    float lastYawDeg = 0.0f;
    float lastPitchDeg = 0.0f;
    float lastRollDeg = 0.0f;

    // Acceleration filter state
    float prevLocalVel[3] = { 0.0f, 0.0f, 0.0f };
    float smoothedSurge = 0.0f;
    float smoothedSway = 0.0f;
    float smoothedHeave = 0.0f;
    bool accelInitialized = false;

    // Suspension tracking
    float lastTyreHeight[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool suspInitialized = false;

    // Airborne / Landing impact tracking
    bool wasAirborne = false;
    int airborneFrames = 0;
    int landingShockFrames = 0;

    // Gear shift on boost / item tracking
    bool wasBoosting = false;
    int boostPulseFrames = 0;
    int itemPulseFrames = 0;
    bool itemIsBoost = false;
    int itemSurgeFrames = 0;

    // Previous speed for wall-hit / collision drop
    float lastSpeed = 0.0f;
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
    mEnableItemFeedback = (bool)CVarGetInteger("gTelemetry.EnableItemFeedback", 1);
    mSpeedFactor = CVarGetFloat("gTelemetry.SpeedFactor", 3.6f);
    mSmoothingAlpha = CVarGetFloat("gTelemetry.SmoothingAlpha", 0.65f);
    mMaxAccel = CVarGetFloat("gTelemetry.MaxAccel", 30.0f);
    std::string ip = CVarGetString("gTelemetry.IP", "127.0.0.1");
    int port = CVarGetInteger("gTelemetry.Port", 20777);

    pImpl->destAddr.sin_family = AF_INET;
    pImpl->destAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &pImpl->destAddr.sin_addr);
}

void TelemetryManager::Update() {
    if (!mIsEnabled) return;

    // Disallow telemetry during Demo / Attract mode
    if (gDemoMode != DEMO_MODE_INACTIVE) {
        pImpl->sessionId = 0;
        pImpl->accelInitialized = false;
        pImpl->suspInitialized = false;
        return;
    }

    if (gGamestate != RACING) {
        pImpl->sessionId = 0; // Reset session when not racing
        pImpl->accelInitialized = false;
        pImpl->suspInitialized = false;
        return;
    }

    Player* p = &gPlayers[0];

    // Check if the race is finished or player has crossed the final finish line
    // When player crosses finish line on lap 3, PLAYER_CINEMATIC_MODE is set (func_8028EEF0)
    // and/or gRaceState advances to RACE_CALCULATE_RANKS / RACE_FINISHED / RACE_EXIT.
    bool raceFinished = (gRaceState >= RACE_CALCULATE_RANKS) ||
                        ((p->type & PLAYER_CINEMATIC_MODE) != 0);

    if (raceFinished) {
        pImpl->sessionId = 0;
        pImpl->accelInitialized = false;
        pImpl->suspInitialized = false;
        return;
    }

    if (pImpl->socket == INVALID_SOCKET) return;

    if (pImpl->sessionId == 0) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        pImpl->sessionId = gen();
        pImpl->sessionStartTime = gCourseTimer;
        pImpl->accelInitialized = false;
        pImpl->suspInitialized = false;
        pImpl->wasAirborne = false;
        pImpl->airborneFrames = 0;
        pImpl->landingShockFrames = 0;
        pImpl->boostPulseFrames = 0;
        pImpl->itemPulseFrames = 0;
        pImpl->itemSurgeFrames = 0;
        pImpl->itemIsBoost = false;
    }

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

    // Fixed 60Hz physics time step
    const float dt = 1.0f / 60.0f;

    // 1. Orientation (Yaw, Pitch, Roll in Degrees)
    // Note: MK64 p->rotation[0] & p->rotation[2] are NEVER updated from 0.
    // Real physical pitch angle comes from front vs rear tyre contact heights:
    // (p->unk_1F8 - p->unk_1FC) / wheelbase.
    // When airborne (p->effects & 8), MK64's internal sprite-renderer multiplies p->slopeAccel
    // by 10 (temp_v0 *= 10), causing pitch to spike to -60° ~ -78° and snap back on landing,
    // which trips SimHub's crash detector ("instant angular changes > 20°").
    // We compute the true physical pitch without the 10x visual exaggeration:
    const float binToDeg = 360.0f / 65536.0f;
    float currentPitchDeg = 0.0f;
    float currentRollDeg  = (float)p->unk_206 * binToDeg;
    float currentYawDeg   = (float)p->rotation[1] * binToDeg;

    if ((p->effects & 8) != 8) {
        currentPitchDeg = (float)p->slopeAccel * binToDeg;
    } else {
        // Calculate true physical pitch during air/jump from velocity trajectory without MK64's 10x visual exaggeration
        float vFwd = p->speed; // forward speed in MK64 units
        float vY = p->velocity[1]; // vertical velocity in MK64 units
        if (vFwd > 0.5f) {
            // Positive pitch = nose down (pitch positive when dropping, negative when climbing)
            float pitchRad = -std::atan2(vY, vFwd);
            currentPitchDeg = pitchRad * (180.0f / 3.14159265f);
        } else {
            currentPitchDeg = (float)p->slopeAccel * binToDeg;
        }
    }

    // Limit pitch to realistic physical bounds (-45° to +45°) to prevent glitch spikes
    currentPitchDeg = std::clamp(currentPitchDeg, -45.0f, 45.0f);
    currentRollDeg  = std::clamp(currentRollDeg,  -45.0f, 45.0f);

    packet.PitchDegrees = currentPitchDeg;
    packet.RollDegrees  = currentRollDeg;
    packet.YawDegrees   = currentYawDeg;

    // 2. Angular Rates (deg/s)
    if (discontinuity || !pImpl->accelInitialized) {
        packet.YawRateDegreesPerSecond = 0.0f;
        packet.PitchRateDegreesPerSecond = 0.0f;
        packet.RollRateDegreesPerSecond = 0.0f;
    } else {
        float dyaw = currentYawDeg - pImpl->lastYawDeg;
        while (dyaw > 180.0f) dyaw -= 360.0f;
        while (dyaw < -180.0f) dyaw += 360.0f;
        packet.YawRateDegreesPerSecond = dyaw / dt;
        packet.PitchRateDegreesPerSecond = (currentPitchDeg - pImpl->lastPitchDeg) / dt;
        packet.RollRateDegreesPerSecond = (currentRollDeg - pImpl->lastRollDeg) / dt;
    }
    pImpl->lastYawDeg = currentYawDeg;
    pImpl->lastPitchDeg = currentPitchDeg;
    pImpl->lastRollDeg = currentRollDeg;

    // 3. Local Velocity calculation (World Velocity -> Local Space)
    float localVelX = p->velocity[0] * p->orientationMatrix[0][0] + p->velocity[1] * p->orientationMatrix[0][1] + p->velocity[2] * p->orientationMatrix[0][2];
    float localVelY = p->velocity[0] * p->orientationMatrix[1][0] + p->velocity[1] * p->orientationMatrix[1][1] + p->velocity[2] * p->orientationMatrix[1][2];
    float localVelZ = p->velocity[0] * p->orientationMatrix[2][0] + p->velocity[1] * p->orientationMatrix[2][1] + p->velocity[2] * p->orientationMatrix[2][2];

    float velScale = mSpeedFactor / 3.6f;
    float lvX = localVelX * velScale;
    float lvY = localVelY * velScale;
    float lvZ = localVelZ * velScale;

    // 4. Airborne & Landing Shock detection
    // In MK64 physics, (p->effects & 8) == 8 is the canonical engine flag for being in the air,
    // and p->hopFrameCounter > 0 tracks driver hops.
    bool isAirborne = ((p->effects & 8) == 8) || (p->hopFrameCounter > 0);

    // Stationary Gate: when parked on the ground, suppress airborne states and residual velocity jitter
    if (p->speed < 0.08f && ((p->effects & 8) == 0)) {
        isAirborne = false;
        lvX = 0.0f;
        lvY = 0.0f;
        lvZ = 0.0f;
    }

    packet.LocalVelocityLateralMps = lvX;
    packet.LocalVelocityUpMps = lvY;
    packet.LocalVelocityForwardMps = lvZ;

    if (isAirborne) {
        pImpl->airborneFrames++;
        pImpl->wasAirborne = true;
    } else {
        if (pImpl->wasAirborne && (pImpl->airborneFrames >= 4 || p->unk_0C2 >= 4 || (p->kartGraphics & POOMP))) {
            // Wheels just touched ground with significant downward speed -> landing impact pulse
            if (p->speed > 0.5f || p->velocity[1] < -0.2f || (p->kartGraphics & POOMP)) {
                pImpl->landingShockFrames = 4; // ~66ms heavy compression shock
            }
        }
        pImpl->airborneFrames = 0;
        pImpl->wasAirborne = false;
    }
    packet.FlightIsOnGround = isAirborne ? 0 : 1;

    // 5. Local Acceleration with EMA filtering and Landing Shock pulse
    if (discontinuity || !pImpl->accelInitialized) {
        pImpl->prevLocalVel[0] = lvX;
        pImpl->prevLocalVel[1] = lvY;
        pImpl->prevLocalVel[2] = lvZ;
        pImpl->smoothedSurge = 0.0f;
        pImpl->smoothedSway = 0.0f;
        pImpl->smoothedHeave = 0.0f;
        pImpl->accelInitialized = true;

        packet.LocalSurgeMs2 = 0.0f;
        packet.LocalSwayMs2 = 0.0f;
        packet.LocalHeaveMs2 = 0.0f;
    } else {
        float rawSurge = (lvZ - pImpl->prevLocalVel[2]) / dt;
        float rawSway  = (lvX - pImpl->prevLocalVel[0]) / dt;
        float rawHeave = (lvY - pImpl->prevLocalVel[1]) / dt;

        if (p->speed < 0.08f && !isAirborne) {
            rawSurge = 0.0f;
            rawSway  = 0.0f;
            rawHeave = 0.0f;
            pImpl->smoothedSurge = 0.0f;
            pImpl->smoothedSway  = 0.0f;
            pImpl->smoothedHeave = 0.0f;
            pImpl->landingShockFrames = 0;
        } else {
            // Apply landing compression shock to vertical heave
            if (pImpl->landingShockFrames > 0) {
                rawHeave += 22.0f; // Downward jolt on touchdown
                pImpl->landingShockFrames--;
            }

            // Apply longitudinal surge kick on Mushroom / Boost item activation
            if (pImpl->itemSurgeFrames > 0) {
                rawSurge += 25.0f; // Vigorous forward acceleration kick (~2.5G)
                pImpl->itemSurgeFrames--;
            }

            // Exponential Moving Average low-pass filter
            float a = mSmoothingAlpha;
            pImpl->smoothedSurge = a * rawSurge + (1.0f - a) * pImpl->smoothedSurge;
            pImpl->smoothedSway  = a * rawSway  + (1.0f - a) * pImpl->smoothedSway;
            pImpl->smoothedHeave = a * rawHeave + (1.0f - a) * pImpl->smoothedHeave;
        }

        float maxA = mMaxAccel;
        packet.LocalSurgeMs2 = std::clamp(pImpl->smoothedSurge, -maxA, maxA);
        packet.LocalSwayMs2  = std::clamp(pImpl->smoothedSway,  -maxA, maxA);
        packet.LocalHeaveMs2 = std::clamp(pImpl->smoothedHeave, -maxA, maxA);

        pImpl->prevLocalVel[0] = lvX;
        pImpl->prevLocalVel[1] = lvY;
        pImpl->prevLocalVel[2] = lvZ;
    }

    // 6. World Position (Double)
    // Convert MK64 units to real-world meters (velScale converts MK64 speed units to m/s,
    // so velScale * 60 frames/sec relates MK64 units to meters).
    // Specifically, 1 MK64 unit is ~0.1m. Without this scaling, moving 10 units in a frame
    // looks to SimHub like 10 meters in 16ms, tripping "Teleportation detected (>10.0m)".
    const double posToMeters = (double)velScale / 60.0;
    packet.VehiclePositionEast = (double)p->pos[0] * posToMeters;
    packet.VehiclePositionUp = (double)p->pos[1] * posToMeters;
    packet.VehiclePositionNorth = (double)-p->pos[2] * posToMeters; // N64 Z is South

    // 7. Race State & Ground Speed
    packet.GroundSpeedKmh = p->speed * mSpeedFactor;
    packet.CompletedLaps = (uint32_t)p->lapCount;
    packet.RacePosition = p->currentRank;
    
    if (gGamestate == RACING) {
        packet.CurrentLapTime = std::max(0.0, (double)gCourseTimer - gTimePlayerLastTouchedFinishLine[0]);
    } else {
        packet.CurrentLapTime = 0.0;
    }
    packet.IsRaceActive = (gGamestate == RACING);

    // 8. Throttle & Brake Pedals
    float throttle = WheelManager_GetThrottle();
    float brake = WheelManager_GetBrake();
    if (throttle <= 0.0f && (gControllerPads[0].button & 0x08000)) throttle = 1.0f; // BTN_A
    if (brake <= 0.0f && (gControllerPads[0].button & 0x04000)) brake = 1.0f;       // BTN_B
    packet.Throttle = throttle;
    packet.Brake = brake;

    // 9. Gear & Engine RPM (with Item / Boost Gear Shift kick)
    bool isBoosting = (p->boostTimer > 0) ||
                      ((p->effects & (BOOST_EFFECT | BOOST_RAMP_WOOD_EFFECT | BOOST_RAMP_ASPHALT_EFFECT | STAR_EFFECT)) != 0) ||
                      ((p->effects & 0x100) != 0); // Mini-turbo release

    if (isBoosting && !pImpl->wasBoosting) {
        pImpl->boostPulseFrames = 15; // ~250ms gear shift pulse
        pImpl->itemSurgeFrames = 6;   // ~100ms surge acceleration kick for motion rig & shaker impact
    }
    pImpl->wasBoosting = isBoosting;

    // Check gear pulses: item pulse or boost pulse
    if (pImpl->boostPulseFrames > 0 || (pImpl->itemPulseFrames > 0 && pImpl->itemIsBoost)) {
        if (pImpl->boostPulseFrames > 0) pImpl->boostPulseFrames--;
        if (pImpl->itemPulseFrames > 0) pImpl->itemPulseFrames--;
        strncpy(packet.Gear, "2", sizeof(packet.Gear)); // Shift to 2 during mushroom / boost
    } else if (pImpl->itemPulseFrames > 0) {
        pImpl->itemPulseFrames--;
        strncpy(packet.Gear, "2", sizeof(packet.Gear)); // Crisp gear pulse on standard item use
    } else if (p->kartProps & MOVE_BACKWARDS) {
        strncpy(packet.Gear, "R", sizeof(packet.Gear));
    } else if (p->speed < 0.08f) {
        strncpy(packet.Gear, "N", sizeof(packet.Gear));
    } else {
        strncpy(packet.Gear, "1", sizeof(packet.Gear));
    }

    packet.EngineMaxRpm = 10000.0f;
    if (isBoosting || pImpl->itemIsBoost) {
        packet.EngineRpm = 9500.0f;
    } else {
        packet.EngineRpm = 1000.0f + std::clamp((p->speed / 8.0f), 0.0f, 1.0f) * 7500.0f;
    }

    // 10. Wheel Slip & Traction Loss
    bool isSpinout = ((p->effects & (0x40 | 0x80 | 0x20000 | 0x10000000)) != 0) ||
                     ((p->kartProps & DRIVING_SPINOUT) != 0) ||
                     ((p->kartGraphics & WHIRRR) != 0) ||
                     ((p->triggers & (START_SPINOUT_TRIGGER | SPINOUT_TRIGGER | DRIVING_SPINOUT_TRIGGER | HIT_BANANA_TRIGGER)) != 0);

    float baseSlip = 0.0f;
    if (isSpinout) {
        baseSlip = 1.0f;
    } else if (p->driftState > 0 || (p->effects & DRIFTING_EFFECT)) {
        baseSlip = (p->driftState >= 2) ? 0.70f : 0.40f;
    } else if (p->speed > 1.0f && std::abs((float)p->unk_0C0) > 2000.0f) {
        baseSlip = std::clamp(std::abs((float)p->unk_0C0) / 10000.0f, 0.0f, 0.5f);
    }
    packet.WheelSlipFrontLeft  = baseSlip * 0.5f;
    packet.WheelSlipFrontRight = baseSlip * 0.5f;
    packet.WheelSlipRearLeft   = baseSlip;
    packet.WheelSlipRearRight  = baseSlip;

    // 11. Per-Wheel Suspension Velocity & Tyre Contact Surface
    auto mapSurface = [](uint8_t st) -> uint16_t {
        switch (st) {
            case AIRBORNE:
                return SIMHUB_SURFACE_NONE;
            case BOOST_RAMP_ASPHALT:
            case BOOST_RAMP_WOOD:
            case TRAIN_TRACK:
            case ROPE_BRIDGE:
            case WOOD_BRIDGE:
            case RAMP:
                return SIMHUB_SURFACE_RUMBLESTRIPS;
            case GRASS:
            case SNOW_OFFROAD:
                return SIMHUB_SURFACE_GRASS;
            case DIRT:
            case DIRT_OFFROAD:
            case SAND:
            case SAND_OFFROAD:
            case WET_SAND:
            case CLIFF:
            case SNOW:
            case ICE:
                return SIMHUB_SURFACE_GRAVEL;
            case ASPHALT:
            case STONE:
            case BRIDGE:
            default:
                return SIMHUB_SURFACE_PRIMARY;
        }
    };

    float suspVel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int t = 0; t < 4; t++) {
        if (!pImpl->suspInitialized || isAirborne || (p->speed < 0.08f)) {
            suspVel[t] = 0.0f;
        } else {
            float dH = (p->tyres[t].baseHeight - pImpl->lastTyreHeight[t]) * velScale;
            suspVel[t] = std::clamp(dH / dt, -20.0f, 20.0f);
        }
        pImpl->lastTyreHeight[t] = p->tyres[t].baseHeight;
    }
    pImpl->suspInitialized = true;

    packet.SuspensionVelocityFrontLeftMps  = suspVel[FRONT_LEFT];
    packet.SuspensionVelocityFrontRightMps = suspVel[FRONT_RIGHT];
    packet.SuspensionVelocityRearLeftMps   = suspVel[BACK_LEFT];
    packet.SuspensionVelocityRearRightMps  = suspVel[BACK_RIGHT];

    if (isAirborne) {
        packet.TyreContactSurfaceFrontLeft  = SIMHUB_SURFACE_NONE;
        packet.TyreContactSurfaceFrontRight = SIMHUB_SURFACE_NONE;
        packet.TyreContactSurfaceRearLeft   = SIMHUB_SURFACE_NONE;
        packet.TyreContactSurfaceRearRight  = SIMHUB_SURFACE_NONE;
    } else {
        packet.TyreContactSurfaceFrontLeft  = mapSurface(p->tyres[FRONT_LEFT].surfaceType);
        packet.TyreContactSurfaceFrontRight = mapSurface(p->tyres[FRONT_RIGHT].surfaceType);
        packet.TyreContactSurfaceRearLeft   = mapSurface(p->tyres[BACK_LEFT].surfaceType);
        packet.TyreContactSurfaceRearRight  = mapSurface(p->tyres[BACK_RIGHT].surfaceType);
    }

    // 12. Custom Fields: EventFlags, SurfaceType
    packet.EventFlags = 0;
    if (p->hopFrameCounter > 0 || isAirborne) packet.EventFlags |= TELEMETRY_EVENT_JUMPING;
    if (isBoosting) packet.EventFlags |= TELEMETRY_EVENT_BOOSTING;

    bool isHit = ((p->effects & (0x400 | 0x4000 | 0x01000000 | HIT_BY_ITEM_EFFECT | LIGHTNING_EFFECT)) != 0) ||
                 ((p->kartGraphics & CRASH) != 0);
    if (isHit) packet.EventFlags |= TELEMETRY_EVENT_HIT;
    if (isSpinout) packet.EventFlags |= TELEMETRY_EVENT_SPINOUT;

    bool wallHit = ((p->unk_046 & 0x20) != 0 || 
                    p->collision.surfaceDistance[0] < -0.2f || 
                    p->collision.surfaceDistance[1] < -0.2f) && (p->speed > 0.8f);
    bool speedDropHit = (pImpl->lastSpeed > 2.0f && (pImpl->lastSpeed - p->speed) > 1.4f);
    pImpl->lastSpeed = p->speed;

    if (wallHit || speedDropHit) packet.EventFlags |= TELEMETRY_EVENT_WALL_HIT;
    if (pImpl->landingShockFrames > 0 && p->speed > 0.5f) packet.EventFlags |= TELEMETRY_EVENT_LANDING;

    packet.SurfaceType = (int16_t)p->surfaceType;

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
        ImGui::TextDisabled("60 Hz sync allows higher alpha (~0.65) for lower latency.");

        if (ImGui::InputFloat("Max Acceleration (m/s^2)", &mMaxAccel, 1.0f, 5.0f, "%.1f")) {
            if (mMaxAccel < 1.0f) mMaxAccel = 1.0f;
            CVarSetFloat("gTelemetry.MaxAccel", mMaxAccel);
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        ImGui::TextDisabled("Clamp acceleration to prevent hitting platform limits. ~30 = ~3G.");

        ImGui::Separator();
        ImGui::Text("Live Telemetry Status:");
        if (gDemoMode != DEMO_MODE_INACTIVE) {
            ImGui::TextDisabled("Status: Inactive (Demo / Attract Mode)");
        } else if (gGamestate == RACING) {
            Player* p = &gPlayers[0];
            bool raceFinished = (gRaceState >= RACE_CALCULATE_RANKS) || ((p->type & PLAYER_CINEMATIC_MODE) != 0);
            if (raceFinished) {
                ImGui::TextDisabled("Status: Inactive (Race Finished / Crossed Finish Line)");
            } else {
                const float binToDeg = 360.0f / 65536.0f;
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Status: Active (In Race)");
                ImGui::Text("Pitch: %.1f deg | Roll: %.1f deg", (float)p->slopeAccel * binToDeg, (float)p->unk_206 * binToDeg);
                ImGui::Text("Yaw: %.1f deg | Speed: %.1f km/h", (float)p->rotation[1] * binToDeg, p->speed * mSpeedFactor);
                ImGui::Text("Airborne: %s | Surface: %d", ((p->effects & 8) == 8) ? "YES" : "No", (int)p->surfaceType);
                ImGui::Text("Effects: 0x%08X", p->effects);
            }
        } else {
            ImGui::TextDisabled("Status: Inactive (Not in active race)");
        }
    }
}

void TelemetryManager::SetItemFeedbackEnabled(bool enabled) {
    mEnableItemFeedback = enabled;
    CVarSetInteger("gTelemetry.EnableItemFeedback", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::TriggerItemFeedback(int itemId) {
    if (!pImpl || !mEnableItemFeedback) return;
    
    // Check if this item is a boost-type item
    bool isBoostItem = (itemId == ITEM_MUSHROOM ||
                        itemId == ITEM_DOUBLE_MUSHROOM ||
                        itemId == ITEM_TRIPLE_MUSHROOM ||
                        itemId == ITEM_SUPER_MUSHROOM ||
                        itemId == ITEM_STAR);

    if (isBoostItem) {
        pImpl->itemIsBoost = true;
        pImpl->itemPulseFrames = 15; // ~250ms strong gear shift pulse
        pImpl->itemSurgeFrames = 6;  // ~100ms forward acceleration jolt (~2.5G)
    } else {
        pImpl->itemIsBoost = false;
        pImpl->itemPulseFrames = 8;  // ~133ms crisp gear shift pulse
        pImpl->itemSurgeFrames = 0;
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

void TelemetryManager_TriggerItemFeedback(int itemId) {
    TelemetryManager::GetInstance()->TriggerItemFeedback(itemId);
}

bool TelemetryManager_IsItemFeedbackEnabled() {
    return TelemetryManager::GetInstance()->IsItemFeedbackEnabled();
}

void TelemetryManager_SetItemFeedbackEnabled(bool enabled) {
    TelemetryManager::GetInstance()->SetItemFeedbackEnabled(enabled);
}
}
