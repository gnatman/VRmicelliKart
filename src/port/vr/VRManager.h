#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "VRTypes.h"

#ifdef __cplusplus
#include <openxr/openxr.h>
#include <vector>
#include <memory>
#include "engine/CoreMath.h"

#ifdef _WIN32
#include <d3d11.h>
#endif

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

#ifdef _WIN32
void VR_SetD3D11Device(void* device, void* context);
#endif

#ifdef __cplusplus
}
#endif
