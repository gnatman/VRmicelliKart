#pragma once

#ifdef __cplusplus
#include <cstdint>

class TelemetryManager {
public:
    static TelemetryManager* GetInstance();

    void Init();
    void Update();
    void DrawSettings();

private:
    TelemetryManager();
    ~TelemetryManager();
    static TelemetryManager* mInstance;

    bool mIsEnabled = false;
    
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

#ifdef __cplusplus
}
#endif
