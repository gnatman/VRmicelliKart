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
    bool IsItemFeedbackEnabled() const { return mEnableItemFeedback; }
    void SetItemFeedbackEnabled(bool enabled);

private:
    TelemetryManager();
    ~TelemetryManager();
    static TelemetryManager* mInstance;

    bool mIsEnabled = false;
    bool mEnableItemFeedback = true;
    float mSpeedFactor = 3.6f;
    float mSmoothingAlpha = 0.3f;
    float mMaxAccel = 30.0f;
    float mBoostSurgeForce = 28.0f;
    
    // Opaque handle for the networking implementation to avoid including winsock2.h here
    struct Impl;
    Impl* pImpl;

    void LoadSettings();
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

#ifdef __cplusplus
}
#endif
