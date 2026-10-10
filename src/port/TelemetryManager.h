#pragma once

#ifdef __cplusplus
#include <cstdint>

class TelemetryManager {
public:
    static TelemetryManager* GetInstance();

    void Init();
    void Update();
    void DrawSettings();
    void TriggerItemFeedback(int itemId);

    // Master enable
    bool IsEnabled() const { return mIsEnabled; }
    void SetEnabled(bool enabled);

    // Item activation feedback (gear-shift pulse & item boost kick)
    bool IsItemFeedbackEnabled() const { return mEnableItemFeedback; }
    void SetItemFeedbackEnabled(bool enabled);

    // Individual Telemetry Event & Channel Toggles
    bool IsBoostSurgeEnabled() const { return mEventBoostSurge; }
    void SetBoostSurgeEnabled(bool enabled);

    bool IsLandingShockEnabled() const { return mEventLandingShock; }
    void SetLandingShockEnabled(bool enabled);

    bool IsWallHitEnabled() const { return mEventWallHit; }
    void SetWallHitEnabled(bool enabled);

    bool IsHitEnabled() const { return mEventHit; }
    void SetHitEnabled(bool enabled);

    bool IsSpinoutEnabled() const { return mEventSpinout; }
    void SetSpinoutEnabled(bool enabled);

    bool IsJumpingEnabled() const { return mEventJumping; }
    void SetJumpingEnabled(bool enabled);

    bool IsSuspensionEnabled() const { return mEventSuspension; }
    void SetSuspensionEnabled(bool enabled);

    bool IsWheelSlipEnabled() const { return mEventWheelSlip; }
    void SetWheelSlipEnabled(bool enabled);

    bool IsSurfaceContactEnabled() const { return mEventSurfaceContact; }
    void SetSurfaceContactEnabled(bool enabled);

    bool IsHopHeaveEnabled() const { return mEventHopHeave; }
    void SetHopHeaveEnabled(bool enabled);
    float GetHopHeaveForce() const { return mHopHeaveForce; }
    void SetHopHeaveForce(float force);
    void TriggerHopHeave();

private:
    TelemetryManager();
    ~TelemetryManager();
    static TelemetryManager* mInstance;

    bool mIsEnabled = false;
    bool mEnableItemFeedback = true;

    // Individual event flags & physics cue toggles
    bool mEventBoostSurge = true;
    bool mEventLandingShock = true;
    bool mEventWallHit = true;
    bool mEventHit = true;
    bool mEventSpinout = true;
    bool mEventJumping = true;
    bool mEventHopHeave = true;
    bool mEventSuspension = true;
    bool mEventWheelSlip = true;
    bool mEventSurfaceContact = true;

    float mSpeedFactor = 3.6f;
    float mSmoothingAlpha = 0.65f;
    float mMaxAccel = 30.0f;
    float mBoostSurgeForce = 28.0f;
    float mHopHeaveForce = 12.0f;
    
    // Opaque handle for the networking implementation to avoid including winsock2.h here
    struct Impl;
    Impl* pImpl;

    void LoadSettings();
    void SaveSettings();
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

void TelemetryManager_Init();
void TelemetryManager_Update();
void TelemetryManager_DrawSettings();
void TelemetryManager_TriggerItemFeedback(int itemId);
bool TelemetryManager_IsItemFeedbackEnabled();
void TelemetryManager_SetItemFeedbackEnabled(bool enabled);
void TelemetryManager_TriggerHopHeave();

#ifdef __cplusplus
}
#endif
