#include "VRMode.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include "racing/math_util.h"
#include "vr/VRSession.h"
#include <cmath>

extern "C" bool VR_IsEnabled() {
    return ::CVarGetInteger("gVR.Enabled", 0) != 0;
}

extern "C" void VR_OverrideCamera(Camera* camera, Player* player, int eye) {
    if (!VR_IsEnabled()) return;

    float worldScale = ::CVarGetFloat("gVR.WorldScale", 1.0f);
    float eyeSep = ::CVarGetFloat("gVR.EyeSeparation", 0.064f);
    if (worldScale == 0.0f) worldScale = 1.0f;

    // Head offset relative to kart center
    float headOffsetX = 0.0f;
    float headOffsetY = 12.0f * worldScale;
    float headOffsetZ = -5.0f * worldScale;

    // Apply OpenXR pose
    float posX, posY, posZ, quatX, quatY, quatZ, quatW;
    VRSession_GetEyePose(eye, &posX, &posY, &posZ, &quatX, &quatY, &quatZ, &quatW);

    // For now, simple stereo offset
    float offset = (eye == 0 ? -eyeSep : eyeSep) * 0.5f * worldScale;
    camera->pos[0] = player->pos[0] + headOffsetX + offset;
    camera->pos[1] = player->pos[1] + headOffsetY;
    camera->pos[2] = player->pos[2] + headOffsetZ;

    // Target is ahead
    float yaw = player->rotation[1] * (M_PI / 32768.0f);
    camera->lookAt[0] = camera->pos[0] + sinf(yaw) * 100.0f;
    camera->lookAt[1] = camera->pos[1];
    camera->lookAt[2] = camera->pos[2] + cosf(yaw) * 100.0f;
}

extern "C" float VR_GetVignetteIntensity(Player* player) {
    if (!VR_IsEnabled()) return 0.0f;
    
    float masterVignette = ::CVarGetFloat("gVR.Vignette", 0.5f);
    float yaw = player->rotation[1] * (M_PI / 32768.0f);
    float cosYaw = std::cos(yaw);
    float sinYaw = std::sin(yaw);
    float latVel = player->velocity[0] * cosYaw - player->velocity[2] * sinYaw;
    float intensity = std::abs(latVel) * 0.01f * masterVignette;
    return std::min(1.0f, intensity);
}

extern "C" {
    void VRSession_GetEyeFov(int eyeIndex, float* left, float* right, float* up, float* down) {
        VRSession::GetInstance().GetEyeFov(eyeIndex, left, right, up, down);
    }

    void VRSession_GetEyePose(int eyeIndex, float* posX, float* posY, float* posZ, float* quatX, float* quatY, float* quatZ, float* quatW) {
        VRSession::GetInstance().GetEyePose(eyeIndex, posX, posY, posZ, quatX, quatY, quatZ, quatW);
    }

    void VRSession_SubmitQuadLayer(void* texture, float width, float height, float distance) {
        VRSession::GetInstance().SubmitQuadLayer(texture, width, height, distance);
    }

    int VRSession_GetCurrentEye() {
        return VRSession::GetInstance().GetCurrentEye();
    }
}
