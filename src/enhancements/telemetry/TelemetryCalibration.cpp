// SpaghettiKart/src/enhancements/telemetry/TelemetryCalibration.cpp
#include "TelemetryCalibration.h"
#include <cmath>
#include <algorithm>
#include <ship/config/ConsoleVariable.h>

void TelemetryCalibration::Calibrate(sTelemetryData& outData, const Player* player, float dt) {
    float speedScale = Ship::CVarGetFloat("gTelemetry.SpeedScale", 0.05f);
    int rpmMin = Ship::CVarGetInteger("gTelemetry.RPMMin", 900);
    int rpmMax = Ship::CVarGetInteger("gTelemetry.RPMMax", 9500);
    int maxGears = Ship::CVarGetInteger("gTelemetry.MaxGears", 4);
    
    // N64 max speed is roughly 100.0f
    float maxN64Speed = 100.0f;
    float speedRatio = std::clamp(player->currentSpeed / maxN64Speed, 0.0f, 1.0f);
    
    // Engine & Gear (Fake for motion rigs)
    outData.sEngineSpeed = rpmMin + speedRatio * (rpmMax - rpmMin);
    outData.sGear = static_cast<int8_t>(std::floor(speedRatio * maxGears) + 1);
    
    // Position (meters)
    outData.sParticipantInfo[0].sWorldPosition[0] = static_cast<int16_t>(player->pos[0] * speedScale);
    outData.sParticipantInfo[0].sWorldPosition[1] = static_cast<int16_t>(player->pos[1] * speedScale);
    outData.sParticipantInfo[0].sWorldPosition[2] = static_cast<int16_t>(player->pos[2] * speedScale);

    // Current Speed in local space Z-axis
    outData.sVelocity[2] = static_cast<int16_t>(player->currentSpeed * speedScale);

    // View participant config
    outData.sViewedParticipantIndex = 0;
    outData.sParticipantInfo[0].sRacePosition = 1;
}
