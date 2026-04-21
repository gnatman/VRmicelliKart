#ifndef WHEEL_FFB_MANAGER_H
#define WHEEL_FFB_MANAGER_H

#include <libultraship.h>

#ifdef _WIN32
#include "libultraship/controller/physicaldevice/DirectInputFFBManager.h"
#endif

class WheelFFBManager {
public:
    static WheelFFBManager* Instance;

    static void Init();
    static void Tick(float dt);

    // Settings
    bool mFFBEnabled = true;
    float mFFBGain = 1.0f;
    float mRackScale = 1.0f;
    float mSpringStrength = 0.5f;
    float mImpactStrength = 1.0f;

private:
    WheelFFBManager();
    ~WheelFFBManager();

    float mImpactRumbleTimer = 0.0f;
};

#endif // WHEEL_FFB_MANAGER_H
