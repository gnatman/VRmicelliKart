// SpaghettiKart/src/enhancements/telemetry/TelemetryCalibration.h
#pragma once
#include "PacketPC2.h"

extern "C" {
#include "include/common_structs.h" // Gives access to Player struct
}

class TelemetryCalibration {
public:
    static void Calibrate(sTelemetryData& outData, const Player* player, float dt);
};
