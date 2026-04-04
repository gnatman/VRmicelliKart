#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "VRTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void VR_Init();
void VR_Update();
void VR_BeginFrame();
void VR_EndFrame();
void VR_Terminate();
bool VR_IsActive();

VREyeData VR_GetEyeData(int eye);
VREyeData VR_GetRigCenterData();
void VR_BindEye(int eye);
void VR_CommitEye(int eye);

#ifdef __cplusplus
}
#endif
