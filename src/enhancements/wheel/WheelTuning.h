#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void WheelTuning_ApplyCurve(int8_t* rawStickX, float currentSpeed);

#ifdef __cplusplus
}
#endif
