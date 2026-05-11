#pragma once

#ifdef __cplusplus

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <SDL2/SDL.h>
#include <libultraship.h>

class WheelManager {
public:
    static WheelManager* GetInstance();

    void Init();
    void Update();
    void ProcessInput(OSContPad* pad);
    void DrawUI();
    void DrawSettings();

    bool IsEnabled();

private:
    WheelManager();
    ~WheelManager();
    static WheelManager* mInstance;

    SDL_Joystick* mJoystick = nullptr;
    int mJoystickIndex = -1;
    std::string mJoystickGuid;

    void OpenJoystick(int index);
    void CloseJoystick();
    void RefreshJoysticks();

    // Mapping and Settings
    int mSteeringAxis = -1;
    int mThrottleAxis = -1;
    int mBrakeAxis = -1;
    int mDriftAxis = -1;
    
    bool mSteeringInvert = false;
    bool mThrottleInvert = false;
    bool mBrakeInvert = false;
    bool mDriftInvert = false;

    float mSteeringSensitivity = 1.0f;
    float mSteeringDeadzone = 0.0f;
    float mSteeringLinearity = 1.0f;
    int16_t mSteeringCenter = 0;
    
    float mThrottleThreshold = 0.5f;
    float mBrakeThreshold = 0.5f;
    float mDriftThreshold = 0.5f;

    std::unordered_map<uint16_t, int> mButtonMap; // N64 Button -> SDL Button Index
    
    // UI state
    bool mIsMappingButton = false;
    uint16_t mMappingButtonBitmask = 0;
    
    bool mIsMappingAxis = false;
    int mMappingAxisType = -1; // 0=Steering, 1=Throttle, 2=Brake

    void LoadSettings();
    void SaveSettings();
};

extern "C" {
#endif

void WheelManager_Init();
void WheelManager_Update();
void WheelManager_ProcessInput(void* pad);
void WheelManager_DrawUI();
void WheelManager_DrawSettings();

#ifdef __cplusplus
}
#endif
