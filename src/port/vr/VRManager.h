#pragma once

#include <openxr/openxr.h>
#include <vector>
#include <memory>
#include "engine/CoreMath.h"

#ifdef __cplusplus
extern "C" {
#endif

void VR_Init();
void VR_Update();
void VR_BeginFrame();
void VR_EndFrame();
void VR_Terminate();
bool VR_IsActive();

struct VREyeData {
    float projectionMatrix[4][4];
    float viewMatrix[4][4];
};

VREyeData VR_GetEyeData(int eye);
void VR_BindEye(int eye);
void VR_CommitEye(int eye);

#ifdef __cplusplus
}
#endif
