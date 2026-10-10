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

    // Boost & surge kick tracking
    bool wasBoosting = false;
    bool wasBoostingSustained = false;
    int boostSurgeFrames = 0;
    int boostSurgeTotalFrames = 45;
    int boostExitFrames = 0;
    int itemPulseFrames = 0;
    bool itemIsBoost = false;

    // Previous speed for wall-hit / collision drop
    float lastSpeed = 0.0f;

    // Hop heave & pitch stabilization
    bool wasHopping = false;
    int hopHeaveFrames = 0;
    int hopLandingFrames = 0;
    float preHopPitchDeg = 0.0f;

    // State for live UI diagnostics
    bool lastIsAirborne = false;
    bool lastIsJumping = false;
    int32_t lastEventFlags = 0;
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
    mEventBoostSurge = (bool)CVarGetInteger("gTelemetry.Event.BoostSurge", 1);
    mEventLandingShock = (bool)CVarGetInteger("gTelemetry.Event.LandingShock", 1);
    mEventWallHit = (bool)CVarGetInteger("gTelemetry.Event.WallHit", 1);
    mEventHit = (bool)CVarGetInteger("gTelemetry.Event.Hit", 1);
    mEventSpinout = (bool)CVarGetInteger("gTelemetry.Event.Spinout", 1);
    mEventJumping = (bool)CVarGetInteger("gTelemetry.Event.Jumping", 1);
    mEventHopHeave = (bool)CVarGetInteger("gTelemetry.Event.HopHeave", 1);
    mEventSuspension = (bool)CVarGetInteger("gTelemetry.Event.Suspension", 1);
    mEventWheelSlip = (bool)CVarGetInteger("gTelemetry.Event.WheelSlip", 1);
    mEventSurfaceContact = (bool)CVarGetInteger("gTelemetry.Event.SurfaceContact", 1);

    mSpeedFactor = CVarGetFloat("gTelemetry.SpeedFactor", 3.6f);
    mSmoothingAlpha = CVarGetFloat("gTelemetry.SmoothingAlpha", 0.65f);
    mMaxAccel = CVarGetFloat("gTelemetry.MaxAccel", 30.0f);
    mBoostSurgeForce = CVarGetFloat("gTelemetry.BoostSurgeForce", 28.0f);
    mHopHeaveForce = CVarGetFloat("gTelemetry.HopHeaveForce", 12.0f);
    std::string ip = CVarGetString("gTelemetry.IP", "127.0.0.1");
    int port = CVarGetInteger("gTelemetry.Port", 20777);

    pImpl->destAddr.sin_family = AF_INET;
    pImpl->destAddr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &pImpl->destAddr.sin_addr);
}

void TelemetryManager::SaveSettings() {
    CVarSetInteger("gTelemetry.Enabled", mIsEnabled ? 1 : 0);
    CVarSetInteger("gTelemetry.EnableItemFeedback", mEnableItemFeedback ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.BoostSurge", mEventBoostSurge ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.LandingShock", mEventLandingShock ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.WallHit", mEventWallHit ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.Hit", mEventHit ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.Spinout", mEventSpinout ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.Jumping", mEventJumping ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.HopHeave", mEventHopHeave ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.Suspension", mEventSuspension ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.WheelSlip", mEventWheelSlip ? 1 : 0);
    CVarSetInteger("gTelemetry.Event.SurfaceContact", mEventSurfaceContact ? 1 : 0);
    CVarSetFloat("gTelemetry.SpeedFactor", mSpeedFactor);
    CVarSetFloat("gTelemetry.SmoothingAlpha", mSmoothingAlpha);
    CVarSetFloat("gTelemetry.MaxAccel", mMaxAccel);
    CVarSetFloat("gTelemetry.BoostSurgeForce", mBoostSurgeForce);
    CVarSetFloat("gTelemetry.HopHeaveForce", mHopHeaveForce);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
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
        pImpl->lastIsAirborne = false;
        pImpl->lastIsJumping = false;
        pImpl->lastEventFlags = 0;
        pImpl->boostSurgeFrames = 0;
        pImpl->boostExitFrames = 0;
        pImpl->wasBoosting = false;
        pImpl->wasBoostingSustained = false;
        pImpl->itemPulseFrames = 0;
        pImpl->itemIsBoost = false;
        pImpl->wasHopping = false;
        pImpl->hopHeaveFrames = 0;
        pImpl->hopLandingFrames = 0;
        pImpl->preHopPitchDeg = 0.0f;
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

    // Detect driver hop (pressing R / drift button sets p->kartHopVelocity > 0.0f)
    // Note: p->hopFrameCounter is an internal engine idle vibration timer that cycles continuously
    // even while stationary, so it must NEVER be used for jumping/airborne detection.
    bool isHopping = (p->kartHopVelocity > 0.0f);

    // Track hop state transitions
    if (isHopping && !pImpl->wasHopping) {
        // Takeoff: capture pre-hop pitch and initialize heave impulse
        pImpl->preHopPitchDeg = pImpl->lastPitchDeg;
        if (mEventHopHeave) {
            pImpl->hopHeaveFrames = 7; // ~116ms upward pop and apex taper
        }
        pImpl->wasHopping = true;
    } else if (!isHopping && pImpl->wasHopping) {
        // Touchdown: trigger gentle landing settle
        pImpl->wasHopping = false;
        if (mEventHopHeave) {
            pImpl->hopLandingFrames = 2; // ~33ms small compression settle
        }
    }

    // 1. Orientation (Yaw, Pitch, Roll in Degrees)
    // Note: MK64 p->rotation[0] & p->rotation[2] are NEVER updated from 0.
    // Real physical pitch angle comes from front vs rear tyre contact heights:
    // (p->unk_1F8 - p->unk_1FC) / wheelbase.
    // When hopping or airborne (p->effects & 8), MK64's internal sprite-renderer multiplies p->slopeAccel
    // by 10 (temp_v0 *= 10), causing pitch to spike to -60° ~ -78° and snap back on landing,
    // which motion platforms tilt backwards (feeling like climbing a mountain or surging forward).
    // During a hop, we freeze pitch to preHopPitchDeg for clean vertical heave without pitch tilt.
    const float binToDeg = 360.0f / 65536.0f;
    float currentPitchDeg = 0.0f;
    float currentRollDeg  = (float)p->unk_206 * binToDeg;
    float currentYawDeg   = (float)p->rotation[1] * binToDeg;

    if (isHopping || pImpl->hopHeaveFrames > 0) {
        // Freeze pitch to pre-hop angle so hopping never causes pitch-back / mountain-climbing tilt
        currentPitchDeg = pImpl->preHopPitchDeg;
    } else if ((p->effects & 8) != 8) {
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

    // In MK64, kartHopVelocity is an independent displacement added directly to posY rather than velocity[1].
    // Include upward hop velocity along the kart's up-vector so LocalVelocityUpMps reflects the hop.
    if (p->kartHopVelocity > 0.0f) {
        localVelY += p->kartHopVelocity * p->orientationMatrix[1][1];
    }

    float velScale = mSpeedFactor / 3.6f;
    float lvX = localVelX * velScale;
    float lvY = localVelY * velScale;
    float lvZ = localVelZ * velScale;

    // 4. Airborne & Landing Shock detection
    bool isAirborne = false;

    if (p->speed < 0.15f) {
        // At near-zero speed (stationary or parked):
        // The kart is resting on the ground unless actively hopping, in free-fall off a ledge,
        // or being picked up by Lakitu.
        if (isHopping) {
            isAirborne = true;
        } else if (p->velocity[1] < -0.4f && p->collision.surfaceDistance[2] > 1.0f) {
            isAirborne = true;
        } else if ((p->lakituProps & 0x02) != 0) {
            isAirborne = true;
        } else {
            isAirborne = false;
        }

        // Clamp stationary velocities and heave jitter
        if (!isAirborne) {
            lvX = 0.0f;
            lvY = 0.0f;
            lvZ = 0.0f;
        }
    } else {
        // In motion: airborne if driver is hopping or if wheels have genuine clearance from the track.
        // We require either hopping, clearance for > 1 frame (p->unk_0C2 >= 2), or noticeable surface distance
        // to filter out 1-frame terrain polygon seam ticks on flat surfaces.
        isAirborne = isHopping || 
                     (((p->effects & 8) != 0) && (p->unk_0C2 >= 2 || p->collision.surfaceDistance[2] > 0.3f || p->velocity[1] < -0.3f));
    }

    packet.LocalVelocityLateralMps = lvX;
    packet.LocalVelocityUpMps = lvY;
    packet.LocalVelocityForwardMps = lvZ;

    if (isAirborne) {
        pImpl->airborneFrames++;
        pImpl->wasAirborne = true;
    } else {
        // Trigger heavy landing shock only for major jumps/falls (ramps, cliffs, or large drops)
        bool isMajorJump = (pImpl->airborneFrames >= 15 || p->unk_0C2 >= 15 || (p->kartGraphics & POOMP) || p->velocity[1] < -0.8f);
        if (mEventLandingShock && pImpl->wasAirborne && isMajorJump) {
            // Wheels just touched ground with significant downward speed -> heavy landing impact pulse
            if (p->speed > 0.5f || p->velocity[1] < -0.4f || (p->kartGraphics & POOMP)) {
                pImpl->landingShockFrames = 4; // ~66ms heavy compression shock (+22.0 m/s^2)
            }
        }
        pImpl->airborneFrames = 0;
        pImpl->wasAirborne = false;
    }
    packet.FlightIsOnGround = isAirborne ? 0 : 1;

    // 5. Local Acceleration with EMA filtering, Landing Shock pulse, and Boost Surge Kick
    bool isBoosting = (p->boostTimer > 0) ||
                      ((p->effects & (BOOST_EFFECT | BOOST_RAMP_WOOD_EFFECT | BOOST_RAMP_ASPHALT_EFFECT | STAR_EFFECT)) != 0) ||
                      ((p->effects & 0x100) != 0); // Mini-turbo release

    if (isBoosting && !pImpl->wasBoosting) {
        pImpl->boostSurgeFrames = 45; // ~750ms onset boost kick
        pImpl->boostSurgeTotalFrames = 45;
    } else if (!isBoosting && pImpl->wasBoostingSustained) {
        pImpl->boostExitFrames = 12; // ~200ms smooth taper off to prevent sudden nose-dive
    }
    pImpl->wasBoosting = isBoosting;
    pImpl->wasBoostingSustained = isBoosting;

    float boostSurge = 0.0f;
    if (mEventBoostSurge) {
        if (pImpl->boostSurgeFrames > 0) {
            // First 15 frames (~250ms): 100% full kick
            // Frames 16-45: smoothly ramp down from 100% to 60% sustained level
            if (pImpl->boostSurgeFrames > 30) {
                boostSurge = mBoostSurgeForce;
            } else {
                float t = (float)pImpl->boostSurgeFrames / 30.0f; // 1.0 down to 0.0
                boostSurge = mBoostSurgeForce * (0.6f + 0.4f * t);
            }
            pImpl->boostSurgeFrames--;
        } else if (isBoosting) {
            // Sustained boost (e.g. Star or prolonged mushroom timer)
            boostSurge = mBoostSurgeForce * 0.6f;
        } else if (pImpl->boostExitFrames > 0) {
            // Smoothly exit after boost ends
            float t = (float)pImpl->boostExitFrames / 12.0f;
            boostSurge = mBoostSurgeForce * 0.6f * t;
            pImpl->boostExitFrames--;
        }
    } else {
        if (pImpl->boostSurgeFrames > 0) pImpl->boostSurgeFrames--;
        if (pImpl->boostExitFrames > 0) pImpl->boostExitFrames--;
    }

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

        if (p->speed < 0.08f && !isAirborne && pImpl->boostSurgeFrames == 0) {
            rawSurge = 0.0f;
            rawSway  = 0.0f;
            rawHeave = 0.0f;
            pImpl->smoothedSurge = 0.0f;
            pImpl->smoothedSway  = 0.0f;
            pImpl->smoothedHeave = 0.0f;
            pImpl->landingShockFrames = 0;
        } else {
            // If exiting boost or boosting, damp negative rawSurge so deceleration from boost speed doesn't jerk the rig forward
            if (mEventBoostSurge && (isBoosting || pImpl->boostExitFrames > 0) && rawSurge < 0.0f) {
                rawSurge *= 0.2f;
            }

            // Apply landing compression shock to vertical heave
            if (mEventLandingShock && pImpl->landingShockFrames > 0) {
                rawHeave += 22.0f; // Downward jolt on touchdown
                pImpl->landingShockFrames--;
            } else if (!mEventLandingShock) {
                pImpl->landingShockFrames = 0;
            }

            // Exponential Moving Average low-pass filter
            float a = mSmoothingAlpha;
            pImpl->smoothedSurge = a * rawSurge + (1.0f - a) * pImpl->smoothedSurge;
            pImpl->smoothedSway  = a * rawSway  + (1.0f - a) * pImpl->smoothedSway;
            pImpl->smoothedHeave = a * rawHeave + (1.0f - a) * pImpl->smoothedHeave;
        }

        // Hop vertical heave impulse calculation
        float hopHeavePulse = 0.0f;
        if (mEventHopHeave) {
            if (pImpl->hopHeaveFrames > 0) {
                // Rising phase of the hop (quick, clean vertical pop)
                // Frames 7..5 (~50ms): full force pop (mHopHeaveForce, default 12 m/s^2)
                // Frames 4..1 (~66ms): tapers smoothly down to 0 at the apex
                if (pImpl->hopHeaveFrames >= 5) {
                    hopHeavePulse = mHopHeaveForce;
                } else {
                    float t = (float)pImpl->hopHeaveFrames / 5.0f;
                    hopHeavePulse = mHopHeaveForce * t;
                }
                pImpl->hopHeaveFrames--;
            } else if (pImpl->hopLandingFrames > 0) {
                // Touchdown settle: small compression bump (~35% of hop force)
                hopHeavePulse = mHopHeaveForce * 0.35f;
                pImpl->hopLandingFrames--;
            }
        } else {
            if (pImpl->hopHeaveFrames > 0) pImpl->hopHeaveFrames--;
            if (pImpl->hopLandingFrames > 0) pImpl->hopLandingFrames--;
        }

        float maxA = mMaxAccel;
        float maxSurge = mEventBoostSurge ? std::max(mMaxAccel, mBoostSurgeForce) : mMaxAccel;
        float maxHeave = mEventHopHeave ? std::max(mMaxAccel, mHopHeaveForce) : mMaxAccel;
        packet.LocalSurgeMs2 = std::clamp(pImpl->smoothedSurge + boostSurge, -maxA, maxSurge);
        packet.LocalSwayMs2  = std::clamp(pImpl->smoothedSway,  -maxA, maxA);
        packet.LocalHeaveMs2 = std::clamp(pImpl->smoothedHeave + hopHeavePulse, -maxA, maxHeave);

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

    // 9. Gear & Engine RPM
    // Simulated gear shift pulse only for standard item release (shells/bananas) if enabled;
    // boosting remains in gear 1 without artificial shift or RPM spikes.
    if (mEnableItemFeedback && pImpl->itemPulseFrames > 0 && !pImpl->itemIsBoost) {
        pImpl->itemPulseFrames--;
        strncpy(packet.Gear, "2", sizeof(packet.Gear)); // Crisp gear pulse on standard item use
    } else if (p->kartProps & MOVE_BACKWARDS) {
        if (pImpl->itemPulseFrames > 0) pImpl->itemPulseFrames--;
        strncpy(packet.Gear, "R", sizeof(packet.Gear));
    } else if (p->speed < 0.08f) {
        if (pImpl->itemPulseFrames > 0) pImpl->itemPulseFrames--;
        strncpy(packet.Gear, "N", sizeof(packet.Gear));
    } else {
        if (pImpl->itemPulseFrames > 0) pImpl->itemPulseFrames--;
        strncpy(packet.Gear, "1", sizeof(packet.Gear));
    }

    packet.EngineMaxRpm = 10000.0f;
    packet.EngineRpm = 1000.0f + std::clamp((p->speed / 12.0f), 0.0f, 1.0f) * 8500.0f;

    // 10. Wheel Slip & Traction Loss
    bool isSpinout = ((p->effects & (0x40 | 0x80 | 0x20000 | 0x10000000)) != 0) ||
                     ((p->kartProps & DRIVING_SPINOUT) != 0) ||
                     ((p->kartGraphics & WHIRRR) != 0) ||
                     ((p->triggers & (START_SPINOUT_TRIGGER | SPINOUT_TRIGGER | DRIVING_SPINOUT_TRIGGER | HIT_BANANA_TRIGGER)) != 0);

    float baseSlip = 0.0f;
    if (mEventWheelSlip) {
        if (isSpinout && mEventSpinout) {
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
    } else {
        packet.WheelSlipFrontLeft  = 0.0f;
        packet.WheelSlipFrontRight = 0.0f;
        packet.WheelSlipRearLeft   = 0.0f;
        packet.WheelSlipRearRight  = 0.0f;
    }

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

    if (mEventSuspension) {
        packet.SuspensionVelocityFrontLeftMps  = suspVel[FRONT_LEFT];
        packet.SuspensionVelocityFrontRightMps = suspVel[FRONT_RIGHT];
        packet.SuspensionVelocityRearLeftMps   = suspVel[BACK_LEFT];
        packet.SuspensionVelocityRearRightMps  = suspVel[BACK_RIGHT];
    } else {
        packet.SuspensionVelocityFrontLeftMps  = 0.0f;
        packet.SuspensionVelocityFrontRightMps = 0.0f;
        packet.SuspensionVelocityRearLeftMps   = 0.0f;
        packet.SuspensionVelocityRearRightMps  = 0.0f;
    }

    if (isAirborne) {
        packet.TyreContactSurfaceFrontLeft  = SIMHUB_SURFACE_NONE;
        packet.TyreContactSurfaceFrontRight = SIMHUB_SURFACE_NONE;
        packet.TyreContactSurfaceRearLeft   = SIMHUB_SURFACE_NONE;
        packet.TyreContactSurfaceRearRight  = SIMHUB_SURFACE_NONE;
    } else if (mEventSurfaceContact) {
        packet.TyreContactSurfaceFrontLeft  = mapSurface(p->tyres[FRONT_LEFT].surfaceType);
        packet.TyreContactSurfaceFrontRight = mapSurface(p->tyres[FRONT_RIGHT].surfaceType);
        packet.TyreContactSurfaceRearLeft   = mapSurface(p->tyres[BACK_LEFT].surfaceType);
        packet.TyreContactSurfaceRearRight  = mapSurface(p->tyres[BACK_RIGHT].surfaceType);
    } else {
        packet.TyreContactSurfaceFrontLeft  = SIMHUB_SURFACE_PRIMARY;
        packet.TyreContactSurfaceFrontRight = SIMHUB_SURFACE_PRIMARY;
        packet.TyreContactSurfaceRearLeft   = SIMHUB_SURFACE_PRIMARY;
        packet.TyreContactSurfaceRearRight  = SIMHUB_SURFACE_PRIMARY;
    }

    // 12. Custom Fields: EventFlags, SurfaceType
    packet.EventFlags = 0;
    bool isJumping = isAirborne || isHopping;
    if (mEventJumping && isJumping) {
        packet.EventFlags |= TELEMETRY_EVENT_JUMPING;
    }
    if (mEventBoostSurge && isBoosting) {
        packet.EventFlags |= TELEMETRY_EVENT_BOOSTING;
    }

    bool isHit = ((p->effects & (0x400 | 0x4000 | 0x01000000 | HIT_BY_ITEM_EFFECT | LIGHTNING_EFFECT)) != 0) ||
                 ((p->kartGraphics & CRASH) != 0);
    if (mEventHit && isHit) {
        packet.EventFlags |= TELEMETRY_EVENT_HIT;
    }

    if (mEventSpinout && isSpinout) {
        packet.EventFlags |= TELEMETRY_EVENT_SPINOUT;
    }

    bool isStaged = (gRaceState < RACE_IN_PROGRESS);
    if (isStaged) {
        pImpl->lastSpeed = p->speed;
    }
    bool wallHit = !isStaged && (((p->unk_046 & 0x20) != 0 || 
                    p->collision.surfaceDistance[0] < -0.2f || 
                    p->collision.surfaceDistance[1] < -0.2f) && (p->speed > 0.8f));
    bool speedDropHit = !isStaged && (pImpl->lastSpeed > 2.0f && (pImpl->lastSpeed - p->speed) > 1.4f);
    pImpl->lastSpeed = p->speed;

    if (mEventWallHit && (wallHit || speedDropHit)) {
        packet.EventFlags |= TELEMETRY_EVENT_WALL_HIT;
    }

    if (mEventLandingShock && pImpl->landingShockFrames > 0 && p->speed > 0.5f) {
        packet.EventFlags |= TELEMETRY_EVENT_LANDING;
    }

    packet.SurfaceType = (int16_t)p->surfaceType;

    // Cache state for live UI monitor
    pImpl->lastIsAirborne = isAirborne;
    pImpl->lastIsJumping = isJumping;
    pImpl->lastEventFlags = packet.EventFlags;

    sendto(pImpl->socket, (const char*)&packet, sizeof(packet), 0, (sockaddr*)&pImpl->destAddr, sizeof(pImpl->destAddr));
}

void TelemetryManager::DrawSettings() {
    bool enabled = (bool)CVarGetInteger("gTelemetry.Enabled", 0);
    if (ImGui::Checkbox("Enable SimHub Telemetry (Master)", &enabled)) {
        SetEnabled(enabled);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Master switch to broadcast UDP telemetry packets to SimHub motion rigs, bass shakers, and dashboards.");

    if (enabled) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Network Configuration:");

        char ipBuf[64];
        std::string ip = CVarGetString("gTelemetry.IP", "127.0.0.1");
        strncpy(ipBuf, ip.c_str(), sizeof(ipBuf) - 1);
        ipBuf[sizeof(ipBuf) - 1] = '\0';
        if (ImGui::InputText("SimHub Target IP", ipBuf, sizeof(ipBuf))) {
            CVarSetString("gTelemetry.IP", ipBuf);
            LoadSettings();
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }

        int port = CVarGetInteger("gTelemetry.Port", 20777);
        if (ImGui::InputInt("SimHub Target Port", &port)) {
            CVarSetInteger("gTelemetry.Port", port);
            LoadSettings();
            Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
        }
        ImGui::TextDisabled("Default Port: 20777 (UDP)");

        if (ImGui::InputFloat("Speed Scaling Factor", &mSpeedFactor, 0.1f, 1.0f, "%.2f")) {
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("MK64 internal speed * factor = GroundSpeedKmh (also scales velocities and displacements).");

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Motion Platform Tuning & Limits:");

        if (ImGui::SliderFloat("Smoothing Filter (Alpha)", &mSmoothingAlpha, 0.05f, 1.0f, "%.2f")) {
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Exponential moving average alpha (0.05 = heavily damped, 1.0 = raw instantaneous). Default ~0.65.");

        if (ImGui::InputFloat("Max Acceleration (m/s^2)", &mMaxAccel, 1.0f, 5.0f, "%.1f")) {
            if (mMaxAccel < 1.0f) mMaxAccel = 1.0f;
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clamps acceleration outputs to prevent hitting motion platform limits (~30 m/s^2 = ~3G).");

        if (ImGui::SliderFloat("Boost Surge Kick Force (m/s^2)", &mBoostSurgeForce, 0.0f, 50.0f, "%.1f")) {
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Magnitude of forward acceleration surge impulse injected during boosts (Mushroom, Star, boost pad, mini-turbo).");

        if (ImGui::SliderFloat("Hop Heave Force (m/s^2)", &mHopHeaveForce, 0.0f, 30.0f, "%.1f")) {
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Peak upward acceleration heave impulse injected during a driver hop (R button). Default 12.0 m/s^2.");

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Individual Telemetry Cues & Events (Enable / Disable):");
        ImGui::TextDisabled("Toggle specific cues and events to isolate, tune, and test each channel on your motion rig or bass shakers.");

        // 1. Boost Surge
        if (ImGui::Checkbox("Enable Boost Surge Kick", &mEventBoostSurge)) {
            SetBoostSurgeEnabled(mEventBoostSurge);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits forward acceleration surge impulse and TELEMETRY_EVENT_BOOSTING flag during mushroom, star, zipper, and mini-turbo boosts.");

        // 2. Landing Shock
        if (ImGui::Checkbox("Enable Landing Impact Shock", &mEventLandingShock)) {
            SetLandingShockEnabled(mEventLandingShock);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits downward heave compression shock and TELEMETRY_EVENT_LANDING flag upon touchdown after jumps and hops.");

        // 3. Wall Collisions
        if (ImGui::Checkbox("Enable Wall Collision Telemetry", &mEventWallHit)) {
            SetWallHitEnabled(mEventWallHit);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits TELEMETRY_EVENT_WALL_HIT flag on track perimeter and obstacle collisions.");

        // 4. Item Attacks & Crashes
        if (ImGui::Checkbox("Enable Item Hit & Crash Telemetry", &mEventHit)) {
            SetHitEnabled(mEventHit);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits TELEMETRY_EVENT_HIT flag when struck by shells, bananas, lightning, or vehicle crash.");

        // 5. Spinouts & Slips
        if (ImGui::Checkbox("Enable Spinout & Traction Loss Telemetry", &mEventSpinout)) {
            SetSpinoutEnabled(mEventSpinout);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits TELEMETRY_EVENT_SPINOUT flag and max rear tire slip when spinning out on oil or bananas.");

        // 6. Jumps & Hops
        if (ImGui::Checkbox("Enable Jump & Hop Telemetry", &mEventJumping)) {
            SetJumpingEnabled(mEventJumping);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits TELEMETRY_EVENT_JUMPING flag during hops and airborne flight.");

        // 7. Hop Vertical Heave Cue
        if (ImGui::Checkbox("Enable Hop Vertical Heave Telemetry", &mEventHopHeave)) {
            SetHopHeaveEnabled(mEventHopHeave);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Injects a clean, quick vertical heave pop and landing settle when performing a hop (R button). Zero pitch or surge kick.");

        // 8. Gear-Shift & Item Activation Pulses
        if (ImGui::Checkbox("Enable Gear-Shift & Item Activation Pulses (ShakeIt)", &mEnableItemFeedback)) {
            SetItemFeedbackEnabled(mEnableItemFeedback);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits simulated gear shift pulse (Gear 1 -> 2) on item launch for tactile gear-shift haptics in ShakeIt Bass Shakers.");

        // 9. Suspension Bump Telemetry
        if (ImGui::Checkbox("Enable Suspension Bump / Road Heave Telemetry", &mEventSuspension)) {
            SetSuspensionEnabled(mEventSuspension);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits per-wheel vertical suspension velocity. Uncheck if you want a smooth motion platform without road heave chatter.");

        // 10. Wheel Slip Telemetry
        if (ImGui::Checkbox("Enable Wheel Slip / Drift Telemetry", &mEventWheelSlip)) {
            SetWheelSlipEnabled(mEventWheelSlip);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits per-wheel slip ratios during cornering, power-slides, and drift mini-turbos for traction loss shakers/actuators.");

        // 11. Tyre Contact Surface Reporting
        if (ImGui::Checkbox("Enable Tyre Contact Surface Telemetry", &mEventSurfaceContact)) {
            SetSurfaceContactEnabled(mEventSurfaceContact);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits track surface types (rumble strips, grass, gravel, dirt, sand) to trigger SimHub road texture effects.");

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Interactive Telemetry Tests:");
        if (ImGui::Button("Test Boost Surge Kick (Mushroom)")) {
            TriggerItemFeedback(ITEM_MUSHROOM);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Triggers an onset forward surge acceleration kick in telemetry.");

        ImGui::SameLine();
        if (ImGui::Button("Test Gear Shift Pulse (Shell)")) {
            TriggerItemFeedback(ITEM_GREEN_SHELL);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Triggers a momentary gear 2 pulse in telemetry for bass shaker gear-shift effects.");

        ImGui::SameLine();
        if (ImGui::Button("Test Hop Heave")) {
            TriggerHopHeave();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Triggers a momentary upward vertical heave pulse in telemetry to test hop actuator response.");

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Live Telemetry Status / Monitor:");
        if (gDemoMode != DEMO_MODE_INACTIVE) {
            ImGui::TextDisabled("Status: Inactive (Demo / Attract Mode)");
        } else if (gGamestate == RACING) {
            Player* p = &gPlayers[0];
            bool raceFinished = (gRaceState >= RACE_CALCULATE_RANKS) || ((p->type & PLAYER_CINEMATIC_MODE) != 0);
            if (raceFinished) {
                ImGui::TextDisabled("Status: Inactive (Race Finished / Crossed Finish Line)");
            } else {
                const float binToDeg = 360.0f / 65536.0f;
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Status: Active (Transmitting at 60 Hz)");
                ImGui::Text("Pitch: %+.1f deg | Roll: %+.1f deg | Yaw: %+.1f deg", (float)p->slopeAccel * binToDeg, (float)p->unk_206 * binToDeg, (float)p->rotation[1] * binToDeg);
                ImGui::Text("Surge: %+.1f m/s^2 | Sway: %+.1f m/s^2 | Heave: %+.1f m/s^2", pImpl->smoothedSurge, pImpl->smoothedSway, pImpl->smoothedHeave);
                ImGui::Text("Airborne: %s | Boosting: %s | Surface: %d", pImpl->lastIsAirborne ? "YES" : "No", (p->boostTimer > 0) ? "YES" : "No", (int)p->surfaceType);

                if (ImGui::TreeNode("Active Event Flags Breakdown")) {
                    ImGui::BulletText("Boosting Surge: %s (%s)", (pImpl->lastEventFlags & TELEMETRY_EVENT_BOOSTING) ? "ACTIVE" : "Idle", mEventBoostSurge ? "Enabled" : "Disabled");
                    ImGui::BulletText("Landing Shock: %s (%s)", (pImpl->lastEventFlags & TELEMETRY_EVENT_LANDING) ? "ACTIVE" : "Idle", mEventLandingShock ? "Enabled" : "Disabled");
                    ImGui::BulletText("Hop Heave Pulse: %s (%s, %.1f m/s^2)", (pImpl->hopHeaveFrames > 0 || pImpl->hopLandingFrames > 0) ? "ACTIVE" : "Idle", mEventHopHeave ? "Enabled" : "Disabled", mHopHeaveForce);
                    ImGui::BulletText("Wall Collision: %s (%s)", (pImpl->lastEventFlags & TELEMETRY_EVENT_WALL_HIT) ? "ACTIVE" : "Idle", mEventWallHit ? "Enabled" : "Disabled");
                    ImGui::BulletText("Item Hit / Crash: %s (%s)", (pImpl->lastEventFlags & TELEMETRY_EVENT_HIT) ? "ACTIVE" : "Idle", mEventHit ? "Enabled" : "Disabled");
                    ImGui::BulletText("Spinout: %s (%s)", (pImpl->lastEventFlags & TELEMETRY_EVENT_SPINOUT) ? "ACTIVE" : "Idle", mEventSpinout ? "Enabled" : "Disabled");
                    ImGui::BulletText("Jumping: %s (%s)", (pImpl->lastEventFlags & TELEMETRY_EVENT_JUMPING) ? "ACTIVE" : "Idle", mEventJumping ? "Enabled" : "Disabled");
                    ImGui::BulletText("Item Pulse: %s (%s)", (pImpl->itemPulseFrames > 0) ? "ACTIVE" : "Idle", mEnableItemFeedback ? "Enabled" : "Disabled");
                    ImGui::TreePop();
                }
            }
        } else {
            ImGui::TextDisabled("Status: Inactive (Not in active race)");
        }
    }
}

void TelemetryManager::SetEnabled(bool enabled) {
    mIsEnabled = enabled;
    CVarSetInteger("gTelemetry.Enabled", enabled ? 1 : 0);
    LoadSettings();
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetItemFeedbackEnabled(bool enabled) {
    mEnableItemFeedback = enabled;
    CVarSetInteger("gTelemetry.EnableItemFeedback", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetBoostSurgeEnabled(bool enabled) {
    mEventBoostSurge = enabled;
    CVarSetInteger("gTelemetry.Event.BoostSurge", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetLandingShockEnabled(bool enabled) {
    mEventLandingShock = enabled;
    CVarSetInteger("gTelemetry.Event.LandingShock", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetWallHitEnabled(bool enabled) {
    mEventWallHit = enabled;
    CVarSetInteger("gTelemetry.Event.WallHit", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetHitEnabled(bool enabled) {
    mEventHit = enabled;
    CVarSetInteger("gTelemetry.Event.Hit", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetSpinoutEnabled(bool enabled) {
    mEventSpinout = enabled;
    CVarSetInteger("gTelemetry.Event.Spinout", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetJumpingEnabled(bool enabled) {
    mEventJumping = enabled;
    CVarSetInteger("gTelemetry.Event.Jumping", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetSuspensionEnabled(bool enabled) {
    mEventSuspension = enabled;
    CVarSetInteger("gTelemetry.Event.Suspension", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetWheelSlipEnabled(bool enabled) {
    mEventWheelSlip = enabled;
    CVarSetInteger("gTelemetry.Event.WheelSlip", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetSurfaceContactEnabled(bool enabled) {
    mEventSurfaceContact = enabled;
    CVarSetInteger("gTelemetry.Event.SurfaceContact", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetHopHeaveEnabled(bool enabled) {
    mEventHopHeave = enabled;
    CVarSetInteger("gTelemetry.Event.HopHeave", enabled ? 1 : 0);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::SetHopHeaveForce(float force) {
    mHopHeaveForce = force;
    CVarSetFloat("gTelemetry.HopHeaveForce", force);
    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void TelemetryManager::TriggerHopHeave() {
    if (!pImpl) return;
    pImpl->preHopPitchDeg = pImpl->lastPitchDeg;
    pImpl->hopHeaveFrames = 7;
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
        pImpl->boostSurgeFrames = 45; // ~750ms forward acceleration kick
        pImpl->boostSurgeTotalFrames = 45;
        pImpl->itemPulseFrames = 0;   // No simulated gear shift for boosts
    } else {
        pImpl->itemIsBoost = false;
        pImpl->itemPulseFrames = 8;   // ~133ms crisp gear shift pulse
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

void TelemetryManager_TriggerHopHeave() {
    TelemetryManager::GetInstance()->TriggerHopHeave();
}
}
