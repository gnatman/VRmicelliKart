#include "FFBEffects.h"
#include "ship/ffb/FFBManager.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include <cmath>

extern "C" void FFBEffects_Update(void* player, float speed, float steeringAngle, int surfaceType, int isColliding) {
    if (!CVarGetInteger("gWheel.Enabled", 0)) return;
    
    auto ffbDevice = Ship::FFBManager::GetInstance().GetDevice();
    if (!ffbDevice) return;
    
    // Centering Spring
    float springGain = CVarGetFloat("gWheel.FFBCenteringSpring", 0.6f);
    float spring = std::abs(steeringAngle) * springGain;
    ffbDevice->SetSpring(spring);
    
    // Collision Constant Force
    if (isColliding) {
        float collisionGain = CVarGetFloat("gWheel.FFBCollisionScale", 1.0f);
        ffbDevice->SetConstantForce(collisionGain);
    } else {
        ffbDevice->SetConstantForce(0.0f);
    }
    
    // Surface Rumble (Periodic)
    if (surfaceType != 0 /* not normal road */) {
        float rumbleGain = CVarGetFloat("gWheel.FFBRumbleScale", 0.5f);
        ffbDevice->PlayPeriodic(10.0f + (speed * 0.1f), rumbleGain);
    } else {
        ffbDevice->PlayPeriodic(0.0f, 0.0f);
    }
}
