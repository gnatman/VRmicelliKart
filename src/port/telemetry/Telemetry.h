#pragma once

#include <cstdint>
#include <vector>
#include "common_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float pos[3];
    float velocity[3];
    float speed;
    float currentSpeed;
    int16_t rotation[3];
    float orientationMatrix[3][3];
    uint32_t magic; // 'MK64'
} TelemetryPacket;

void Telemetry_Init();
void Telemetry_Update(Player* player);
void Telemetry_Terminate();

#ifdef __cplusplus
}
#endif
