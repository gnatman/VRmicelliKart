#include "WheelTuning.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include <cmath>
#include <algorithm>

extern "C" void WheelTuning_ApplyCurve(int8_t* rawStickX, float currentSpeed) {
    float linearity = CVarGetFloat("gWheel.Linearity", 0.85f);
    float norm = *rawStickX / 127.0f;
    float sign = norm > 0 ? 1.0f : -1.0f;
    
    // Power curve for linearity
    float curved = std::pow(std::abs(norm), linearity);
    
    *rawStickX = (int8_t)(sign * curved * 127.0f);
}
