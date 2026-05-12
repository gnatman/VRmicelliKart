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

    SDL_Haptic* mHaptic = nullptr;
    int mConstantEffectId = -1;
    int mSineEffectId = -1;
    bool mHapticRumbleSupported = false;

    void OpenJoystick(int index);
    void CloseJoystick();
    void RefreshJoysticks();
    void UpdateFFB();

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

    enum class MappingType {
        None,
        Button,
        Hat
    };

    struct InputMapping {
        MappingType type = MappingType::None;
        int index = -1; // Button index or Hat index
        uint8_t hatValue = 0; // SDL_HAT_UP, etc.
    };

    std::unordered_map<uint16_t, std::vector<InputMapping>> mButtonMap; // N64 Button -> List of Mappings
    
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
