#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#endif
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>
#include <memory>
#include <functional>
#include "VRSwapchain.h"

struct VRPose {
    XrPosef head;
    XrPosef eyes[2];
    XrFovf fov[2];
};

class VRSession {
public:
    static VRSession& GetInstance();
    void Init(ID3D11Device* device);
    void Destroy();

    bool WaitFrame(VRPose& outPose);
    void BeginFrame();
    void EndFrame();
    void RenderEye(int eyeIndex, std::function<void(ID3D11RenderTargetView*)> renderFunc);
    void SubmitQuadLayer(void* texture, float width, float height, float distance);

    void GetEyeFov(int eyeIndex, float* left, float* right, float* up, float* down);
    void GetEyePose(int eyeIndex, float* posX, float* posY, float* posZ, float* quatX, float* quatY, float* quatZ, float* quatW);
    int GetCurrentEye() const { return mCurrentEye; }
    void SetCurrentEye(int eye) { mCurrentEye = eye; }

private:
    XrInstance mInstance{XR_NULL_HANDLE};
    XrSystemId mSystemId{XR_NULL_SYSTEM_ID};
    XrSession mSession{XR_NULL_HANDLE};
    XrSpace mStageSpace{XR_NULL_HANDLE};
    XrView mViews[2];
    std::unique_ptr<VRSwapchain> mSwapchains[2];
    int mCurrentEye{0};
};

#ifdef __cplusplus
extern "C" {
#endif

void VRSession_GetEyeFov(int eyeIndex, float* left, float* right, float* up, float* down);
void VRSession_GetEyePose(int eyeIndex, float* posX, float* posY, float* posZ, float* quatX, float* quatY, float* quatZ, float* quatW);
void VRSession_SubmitQuadLayer(void* texture, float width, float height, float distance);
int VRSession_GetCurrentEye();

#ifdef __cplusplus
}
#endif
