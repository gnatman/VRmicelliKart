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

    // Phase 4a: OpenXR initialization will go here
    printf("[VR] VR Mode initialized (Phase 3 — Desktop Only)\n");
}

void VR_Shutdown(void) {
    if (!sVRInitialized) {
        return;
    }
    sVRInitialized = false;

    // Phase 4a: OpenXR shutdown will go here
    printf("[VR] VR Mode shut down\n");
}

void VR_PreFrame(void) {
    if (!VR_IsEnabled()) {
        return;
    }

    // Phase 3: Use mouse delta for head rotation (right-click drag)
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

    // Phase 4a: xrWaitFrame / xrBeginFrame / xrLocateViews will go here
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
