#include <libultraship.h>
#include <ship/window/Window.h>
#include "port/Engine.h"

extern "C" {
#include <macros.h>
#include <defines.h>
#include <common_structs.h>
#include "main.h"
#include "enhancements/vr/VRMode.h"
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
 * VR Mode — Phase 3 Implementation (Desktop Only)
 *
 * Provides a first-person camera on the kart driven by mouse input.
 * No HMD or OpenXR is used in this phase.
 */

// Head orientation state (radians)
static float sVRHeadYaw = 0.0f;
static float sVRHeadPitch = 0.0f;
static bool  sVRInitialized = false;

// Mouse sensitivity for desktop VR camera
static const float kMouseSensitivityX = 0.004f;
static const float kMouseSensitivityY = 0.004f;

// Pitch clamp (avoid looking straight up/down)
static const float kMaxPitch = 1.4f;  // ~80 degrees

int VR_IsEnabled(void) {
    // VR requires:
    // 1. CVar enabled
    // 2. Single player mode
    // 3. Currently in RACING game state
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
    sVRInitialized = true;

#ifdef LUS_ENABLE_VR
    auto* fastWnd = dynamic_cast<Fast::Fast3dWindow*>(GameEngine::Instance->context->GetWindow().get());
    if (fastWnd) {
        auto* renderingApi = dynamic_cast<Fast::GfxRenderingAPIDX11*>(fastWnd->GetRenderingApi());
        if (renderingApi) {
            sVRSession = std::make_unique<LUS::VRSession>(renderingApi->GetDevice());
            if (sVRSession->Init()) {
                printf("[VR] OpenXR Session initialized successfully.\n");
            } else {
                printf("[VR] OpenXR Session failed to initialize.\n");
                sVRSession.reset();
            }
        }
    }
#endif

    printf("[VR] VR Mode initialized\n");
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

    printf("[VR] VR Mode shut down\n");
}

void VR_PreFrame(void) {
    if (!VR_IsEnabled()) {
        return;
    }

#ifdef LUS_ENABLE_VR
    if (sVRSession && sVRSession->IsSessionActive()) {
        sVRSession->WaitFrame(sVRPose);
        sVRSession->BeginFrame();
        // In Phase 4b, we'll extract yaw/pitch from sVRPose.head
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

void VR_Recenter(void) {
    sVRHeadYaw = 0.0f;
    sVRHeadPitch = 0.0f;
}

void VR_PostFrame(void) {
    if (!VR_IsEnabled()) {
        return;
    }
#ifdef LUS_ENABLE_VR
    if (sVRSession && sVRSession->IsSessionActive()) {
        // We'll use XR_ENVIRONMENT_BLEND_MODE_OPAQUE for normal VR
        sVRSession->EndFrame(XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
    }
#endif
}

#ifdef LUS_ENABLE_VR
LUS::VRSession* VR_GetSession() { return sVRSession.get(); }
LUS::VRPose* VR_GetPose() { return &sVRPose; }
#endif
