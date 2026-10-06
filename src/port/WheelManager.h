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
    
    // Native wheel steering — full float precision [-1.0, 1.0]
    float GetNativeSteer() const { return mNativeSteer; }
    float GetThrottle() const { return mNativeThrottle; }
    float GetBrake() const { return mNativeBrake; }
    void DrawSettings();
    void DrawHapticsSettings();

    bool IsFFBEnabled() const { return mFFBEnabled; }
    void SetFFBEnabled(bool enabled) { mFFBEnabled = enabled; SaveSettings(); }

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
    int mFFBMasterGain = 100;
    std::string mHapticErrorStr;

    // Haptics & Force Feedback Enable Switches
    bool mFFBEnabled = true;
    bool mFFBEnableCentering = true;
    bool mFFBEnableLateral = true;
    bool mFFBEnableCollisionJolts = true;
    bool mFFBEnableOffroadRumble = true;
    bool mFFBEnableSpinoutShake = true;

    void OpenJoystick(int index);
    void CloseJoystick();
    void RefreshJoysticks();
    void UpdateFFB();
    void InitHapticEffects();
    void ReInitHapticEffects();

    // Telemetry & Diagnostics
    float mCurrentAngleDeg = 0.0f;
    int16_t mLastCommandedForce = 0;
    int16_t mLastSoftLockForce = 0;
    int16_t mLastCenteringForce = 0;
    int16_t mLastLateralForce = 0;
    int16_t mLastJoltForce = 0;
    int16_t mLastRumbleForce = 0;
    int16_t mLastSineMagnitude = 0;
    int mJoltFramesRemaining = 0;
    bool mIsSpinoutActive = false;
    bool mIsOffroadActive = false;
    float mLastPlayerSpeed = 0.0f;
    uint32_t mLastPlayerEffects = 0;
    uint32_t mLastPlayerTriggers = 0;
    int mLastFFBUpdateResult = 0;
    int mLastFFBEffectStatus = 0;
    int mConsecutiveFFBErrors = 0;
    int mAutoRearmCount = 0;
    int mTestJoltFrames = 0;
    bool mTestShakeActive = false;
    std::string mLastFFBError;
    uint8_t mConstantDirectionType = SDL_HAPTIC_STEERING_AXIS;
    bool mFFBInvert = false;

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
    float mSteeringSaturation = 1.0f;
    float mSteeringSCurve = 0.0f;
    int16_t mSteeringCenter = 0;
    bool mCombineInputs = true;
    
    // Native high-resolution steer value, set each frame in ProcessInput
    float mNativeSteer = 0.0f;
    float mNativeThrottle = 0.0f;
    float mNativeBrake = 0.0f;
    
    // Rotation Limit / Soft Lock (for Direct Drive & FFB wheels)
    bool mSoftLockEnabled = true;
    int mHardwareDOR = 900;       // Operating range in degrees set in wheel driver
    float mSoftLockAngle = 90.0f; // Target max rotation on each side (90 deg = 180 deg total)
    float mSoftLockStiffness = 1.0f;
    
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
void WheelManager_DrawHapticsSettings();

// Native wheel steering API — bypasses N64 stick quantization
// Returns the wheel's post-curve steering as a float in [-1.0, 1.0],
// or 0.0 if wheel is not active.
float WheelManager_GetNativeSteer();
float WheelManager_GetThrottle();
float WheelManager_GetBrake();
// Returns 1 if wheel is enabled AND native steering mode is active, 0 otherwise.
int   WheelManager_IsNativeSteerActive();

#ifdef __cplusplus
}
#endif
