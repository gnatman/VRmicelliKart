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

private:
    TelemetryManager();
    ~TelemetryManager();
    static TelemetryManager* mInstance;

    bool mIsEnabled = false;
    float mSpeedFactor = 3.6f;
    float mSmoothingAlpha = 0.3f;
    float mMaxAccel = 30.0f;
    
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

#ifdef __cplusplus
}
#endif
