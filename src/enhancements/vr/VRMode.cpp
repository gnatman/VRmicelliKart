#include <libultraship.h>
#include <spdlog/spdlog.h>
#include <ship/window/Window.h>
#include "port/Engine.h"

#include "enhancements/vr/VRMode.h"
extern "C" {
#include <macros.h>
#include <defines.h>
#include <common_structs.h>
#include "main.h"
}

#include <cmath>

#ifdef LUS_ENABLE_VR
#include "vr/VRSession.h"
#include "fast/backends/gfx_direct3d_common.h"
#include "fast/Fast3dWindow.h"

static std::unique_ptr<LUS::VRSession> sVRSession;
static LUS::VRPose sVRPose;
#endif

/**
 * VR Mode — Cockpit Implementation
 *
 * Provides a first-person camera on the kart driven by OpenXR HMD input.
 */

// Head orientation state (radians)
static float sVRHeadYaw = 0.0f;
static float sVRHeadPitch = 0.0f;
static float sVRHeadRoll = 0.0f;
static float sVRHeadPos[3] = { 0.0f, 0.0f, 0.0f };
static bool  sVRInitialized = false;

// Mouse sensitivity for desktop VR camera (Phase 3 fallback)
static const float kMouseSensitivityX = 0.004f;
static const float kMouseSensitivityY = 0.004f;

// Pitch clamp (avoid looking straight up/down)
static const float kMaxPitch = 1.4f;  // ~80 degrees

int VR_IsEnabled(void) {
    if (!CVarGetInteger("gVR.Enabled", 0)) {
        return 0;
    }
    if (gActiveScreenMode != SCREEN_MODE_1P) {
        return 0;
    }
    if (gGamestate != RACING) {
        return 0;
    }
    return 1;
}

void VR_Init(void) {
    if (sVRInitialized) {
        return;
    }
    sVRHeadYaw = 0.0f;
    sVRHeadPitch = 0.0f;
    sVRHeadRoll = 0.0f;
    sVRHeadPos[0] = 0.0f; sVRHeadPos[1] = 0.0f; sVRHeadPos[2] = 0.0f;
    sVRInitialized = true;

#ifdef LUS_ENABLE_VR
    auto* fastWnd = dynamic_cast<Fast::Fast3dWindow*>(GameEngine::Instance->context->GetWindow().get());
    if (fastWnd) {
        auto* renderingApi = dynamic_cast<Fast::GfxRenderingAPIDX11*>(fastWnd->GetRenderingApi());
        if (renderingApi) {
            sVRSession = std::make_unique<LUS::VRSession>(renderingApi->GetDevice());
            if (sVRSession->Init()) {
                SPDLOG_INFO("[VR] OpenXR Session initialized successfully.");
            } else {
                SPDLOG_ERROR("[VR] OpenXR Session failed to initialize.");
                sVRSession.reset();
            }
        }
    }
#endif

    SPDLOG_INFO("[VR] VR Mode initialized");
}

void VR_Shutdown(void) {
    if (!sVRInitialized) {
        return;
    }
    sVRInitialized = false;

#ifdef LUS_ENABLE_VR
    if (sVRSession) {
        sVRSession->Shutdown();
        sVRSession.reset();
    }
#endif

    SPDLOG_INFO("[VR] VR Mode shut down");
}

static float sCenterPosX = 0.0f;
static float sCenterPosY = 0.0f;
static float sCenterPosZ = 0.0f;
static float sCenterYaw = 0.0f;

void VR_PreFrame(void) {
    if (!VR_IsEnabled()) {
        return;
    }

#ifdef LUS_ENABLE_VR
    if (sVRSession && sVRSession->IsSessionActive()) {
        sVRSession->WaitFrame(sVRPose);
        sVRSession->BeginFrame();
        
        // Extract relative pose for player 0 (head)
        XrPosef headPose = sVRPose.eyes[0]; // Simplified: use left eye for base orientation
        
        // Relative orientation: q_new = q_center_inv * q
        float cy = cosf(-sCenterYaw * 0.5f);
        float sy = sinf(-sCenterYaw * 0.5f);
        XrQuaternionf q = headPose.orientation;
        
        float qx = q.x * cy + q.z * sy;
        float qy = q.y * cy + q.w * sy;
        float qz = q.z * cy - q.x * sy;
        float qw = q.w * cy - q.y * sy;
        
        // Convert quaternion to Euler (Yaw, Pitch, Roll)
        // Pitch (x-axis rotation)
        float sinp = 2.0f * (qw * qx - qy * qz);
        if (fabsf(sinp) >= 1.0f)
            sVRHeadPitch = copysignf(M_PI / 2.0f, sinp);
        else
            sVRHeadPitch = asinf(sinp);

        // Yaw (y-axis rotation) - Negate for MK64 coordinate system
        float siny_cosp = 2.0f * (qw * qy + qz * qx);
        float cosy_cosp = 1.0f - 2.0f * (qx * qx + qy * qy);
        sVRHeadYaw = -atan2f(siny_cosp, cosy_cosp);

        // Roll (z-axis rotation)
        float sinr_cosp = 2.0f * (qw * qz + qx * qy);
        float cosr_cosp = 1.0f - 2.0f * (qz * qz + qx * qx);
        sVRHeadRoll = atan2f(sinr_cosp, cosr_cosp);
        
        // Relative position: p_new = rotate(p - sCenterPos, -sCenterYaw)
        float dx = headPose.position.x - sCenterPosX;
        float dy = headPose.position.y - sCenterPosY;
        float dz = headPose.position.z - sCenterPosZ;
        
        float cosYaw = cosf(-sCenterYaw);
        float sinYaw = sinf(-sCenterYaw);
        sVRHeadPos[0] = dx * cosYaw - dz * sinYaw;
        sVRHeadPos[1] = dy;
        sVRHeadPos[2] = dx * sinYaw + dz * cosYaw;
    }
#endif

    // Phase 3 fallback: Use mouse delta for head rotation
    auto wnd = GameEngine::Instance->context->GetWindow();
    Ship::Coords mouse = wnd->GetMouseDelta();

    if (wnd->GetMouseState(Ship::LUS_MOUSE_BTN_RIGHT)) {
        sVRHeadYaw -= mouse.x * kMouseSensitivityX;
        sVRHeadPitch -= mouse.y * kMouseSensitivityY;

        // Clamp pitch
        if (sVRHeadPitch > kMaxPitch) {
            sVRHeadPitch = kMaxPitch;
        }
        if (sVRHeadPitch < -kMaxPitch) {
            sVRHeadPitch = -kMaxPitch;
        }
    }
}

int VR_GetCameraMode(void) {
    return CVarGetInteger("gVR.CameraMode", VR_CAMERA_COCKPIT);
}

float VR_GetHeadYaw(void) {
    return sVRHeadYaw;
}

float VR_GetHeadPitch(void) {
    return sVRHeadPitch;
}

float VR_GetHeadRoll(void) {
    return sVRHeadRoll;
}

void VR_GetHeadPos(float* x, float* y, float* z) {
    if (x) *x = sVRHeadPos[0];
    if (y) *y = sVRHeadPos[1];
    if (z) *z = sVRHeadPos[2];
}

void VR_Recenter(void) {
    sVRHeadYaw = 0.0f;
    sVRHeadPitch = 0.0f;

#ifdef LUS_ENABLE_VR
    if (!sVRSession || !sVRSession->IsSessionActive()) return;
    
    XrPosef pose = sVRPose.eyes[0];
    
    sCenterPosX = pose.position.x;
    sCenterPosY = pose.position.y;
    sCenterPosZ = pose.position.z;

    XrQuaternionf q = pose.orientation;
    sCenterYaw = atan2f(2.0f * (q.y * q.w - q.x * q.z), 1.0f - 2.0f * (q.y * q.y + q.z * q.z));
#endif
}

void VR_PostFrame(void) {
    if (!VR_IsEnabled()) {
        return;
    }
#ifdef LUS_ENABLE_VR
    if (sVRSession && sVRSession->IsSessionActive()) {
        sVRSession->EndFrame(XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
    }
#endif
}

#ifdef LUS_ENABLE_VR
LUS::VRSession* VR_GetSession() { return sVRSession.get(); }
LUS::VRPose* VR_GetPose() { return &sVRPose; }
#endif

static int sCurrentEye = 0;

void VR_SetCurrentEye(int eye) {
    sCurrentEye = eye;
}

int VR_GetCurrentEye(void) {
    return sCurrentEye;
}

int VR_BuildPerspectiveMatrix(float mf[4][4], unsigned short* perspNorm, float nearPlane, float farPlane, float scale) {
#ifdef LUS_ENABLE_VR
    if (!sVRSession || !sVRSession->IsSessionActive()) return 0;
    
    XrFovf fov = sVRPose.fov[sCurrentEye];
    float tanLeft = tanf(fov.angleLeft);
    float tanRight = tanf(fov.angleRight);
    float tanUp = tanf(fov.angleUp);
    float tanDown = tanf(fov.angleDown);

    float tanWidth = tanRight - tanLeft;
    float tanHeight = tanUp - tanDown;

    mf[0][0] = 2.0f / tanWidth;
    mf[0][1] = 0.0f;
    mf[0][2] = 0.0f;
    mf[0][3] = 0.0f;

    mf[1][0] = 0.0f;
    mf[1][1] = 2.0f / tanHeight;
    mf[1][2] = 0.0f;
    mf[1][3] = 0.0f;

    mf[2][0] = (tanRight + tanLeft) / tanWidth;
    mf[2][1] = (tanUp + tanDown) / tanHeight;
    mf[2][2] = (nearPlane + farPlane) / (nearPlane - farPlane);
    mf[2][3] = -1.0f;

    mf[3][0] = 0.0f;
    mf[3][1] = 0.0f;
    mf[3][2] = 2.0f * nearPlane * farPlane / (nearPlane - farPlane);
    mf[3][3] = 0.0f;

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            mf[row][col] *= scale;
        }
    }

    if (perspNorm != nullptr) {
        if (nearPlane + farPlane <= 2.0) {
            *perspNorm = 65535;
        } else {
            *perspNorm = (unsigned short)(((double)(1 << 17)) / (nearPlane + farPlane));
            if (*perspNorm <= 0) {
                *perspNorm = 1;
            }
        }
    }
    return 1;
#else
    return 0;
#endif
}

int VR_ApplyEyeTransform(float mf[4][4]) {
#ifdef LUS_ENABLE_VR
    if (!sVRSession || !sVRSession->IsSessionActive()) return 0;

    XrPosef pose = sVRPose.eyes[sCurrentEye];
    
    // Position offset relative to center
    float dx = pose.position.x - sCenterPosX;
    float dy = pose.position.y - sCenterPosY;
    float dz = pose.position.z - sCenterPosZ;

    float cosYaw = cosf(-sCenterYaw);
    float sinYaw = sinf(-sCenterYaw);
    float recenteredX = dx * cosYaw - dz * sinYaw;
    float recenteredZ = dx * sinYaw + dz * cosYaw;
    float recenteredY = dy;

    float worldScale = CVarGetFloat("gVR.WorldScale", 50.0f) * CVarGetFloat("gVR.IPDScale", 1.0f);
    
    // Apply position offset (IPD and head movement) along the matrix's basis vectors
    // mf[0] is Right, mf[1] is Up, mf[2] is Look
    float offsetX = recenteredX * worldScale;
    float offsetY = recenteredY * worldScale;
    float offsetZ = recenteredZ * worldScale;

    // Translation part of the view matrix is mf[3]
    // mf[3] = -Eye dot Basis
    // To move the eye by p, we do Eye' = Eye + p.
    // T' = -(Eye + p) dot Basis = T - (p dot Basis)
    
    mf[3][0] -= (offsetX * mf[0][0] + offsetY * mf[1][0] + offsetZ * mf[2][0]);
    mf[3][1] -= (offsetX * mf[0][1] + offsetY * mf[1][1] + offsetZ * mf[2][1]);
    mf[3][2] -= (offsetX * mf[0][2] + offsetY * mf[1][2] + offsetZ * mf[2][2]);

    // Handle Roll (Pitch and Yaw are handled in VRCamera.cpp)
    // Extract roll from headset orientation
    // (We reuse the Euler extraction logic if needed, but for now we skip Roll to simplify)
    
    return 1;
#else
    return 0;
#endif
}
