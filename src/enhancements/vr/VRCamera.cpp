#include <libultraship.h>

extern "C" {
#include <macros.h>
#include <defines.h>
#include <common_structs.h>
#include <libc/math.h>
#include "main.h"
#include "camera.h"
#include "enhancements/vr/VRMode.h"
#include "enhancements/vr/VRCamera.h"
}

#include <cmath>

/**
 * VR Camera — Phase 3 Implementation
 *
 * Overrides the third-person camera with a first-person view anchored to the
 * player's kart. Head orientation is driven by mouse input (Phase 3) and will
 * be replaced by OpenXR head tracking in Phase 4b.
 *
 * Camera placement offsets per mode:
 *   - Cockpit: Slightly above and behind the kart center (driver's seat)
 *   - Hood:    At the front of the kart, low
 *   - Chase:   Behind and above the kart (like a close third-person)
 */

// Height offset above the kart position for the driver's eye level
static const float kCockpitHeightOffset = 8.0f;
// Forward offset from kart center
static const float kCockpitForwardOffset = 0.0f;

// Hood cam offsets
static const float kHoodHeightOffset = 4.0f;
static const float kHoodForwardOffset = 12.0f;

// Chase cam offsets
static const float kChaseHeightOffset = 16.0f;
static const float kChaseBackOffset = -25.0f;

void VR_OverrideCamera(Camera* camera, Player* player, int eye) {
    (void)eye; // Phase 3: mono rendering only

    if (player == NULL || camera == NULL) {
        return;
    }

    float headYaw = VR_GetHeadYaw();
    float headPitch = VR_GetHeadPitch();
    int cameraMode = VR_GetCameraMode();

    // Get kart's forward direction from player rotation
    // player->rotation[1] is the yaw in N64 angle units (0-65535 = 0-360°)
    float kartYaw = (float)player->rotation[1] * (2.0f * M_PI / 65536.0f);

    // Kart forward vector (in N64 space: X is right, Y is up, Z is forward)
    float kartForwardX = sinf(kartYaw);
    float kartForwardZ = cosf(kartYaw);

    // Kart right vector
    float kartRightX = cosf(kartYaw);
    float kartRightZ = -sinf(kartYaw);

    // Determine position offsets based on camera mode
    float heightOffset, forwardOffset, sideOffset;
    switch (cameraMode) {
        case VR_CAMERA_HOOD:
            heightOffset = kHoodHeightOffset;
            forwardOffset = kHoodForwardOffset;
            sideOffset = 0.0f;
            break;
        case VR_CAMERA_CHASE:
            heightOffset = kChaseHeightOffset;
            forwardOffset = kChaseBackOffset;
            sideOffset = 0.0f;
            break;
        case VR_CAMERA_COCKPIT:
        default:
            heightOffset = kCockpitHeightOffset;
            forwardOffset = kCockpitForwardOffset;
            sideOffset = 0.0f;
            break;
    }

    // Position the camera on the kart
    camera->pos[0] = player->pos[0]
                    + kartForwardX * forwardOffset
                    + kartRightX * sideOffset;
    camera->pos[1] = player->pos[1] + heightOffset;
    camera->pos[2] = player->pos[2]
                    + kartForwardZ * forwardOffset
                    + kartRightZ * sideOffset;

    // Compose head orientation with kart yaw
    // Final yaw = kart yaw + head yaw (from mouse in Phase 3, from HMD in Phase 4b)
    float finalYaw = kartYaw + headYaw;

    // Build look direction from combined yaw and head pitch
    float lookDirX = sinf(finalYaw) * cosf(headPitch);
    float lookDirY = sinf(headPitch);
    float lookDirZ = cosf(finalYaw) * cosf(headPitch);

    // LookAt point = pos + look direction * some distance
    float lookDistance = 50.0f;
    camera->lookAt[0] = camera->pos[0] + lookDirX * lookDistance;
    camera->lookAt[1] = camera->pos[1] + lookDirY * lookDistance;
    camera->lookAt[2] = camera->pos[2] + lookDirZ * lookDistance;

    // Up vector is always world up for Phase 3
    // Phase 5 may tilt this for banking/comfort
    camera->up[0] = 0.0f;
    camera->up[1] = 1.0f;
    camera->up[2] = 0.0f;

    // Convert to N64 rotation format for compatibility with rendering pipeline
    camera->rot[1] = (s16)(finalYaw * 65536.0f / (2.0f * M_PI));
    camera->rot[0] = (s16)(headPitch * 65536.0f / (2.0f * M_PI));
    camera->rot[2] = 0;

    // Override field of view for VR
    // Phase 3: Use a wider FOV for the first-person feel
    // Phase 4b: This will be overridden by OpenXR asymmetric frustums
    camera->fieldOfView = CVarGetFloat("gVR.FOV", 90.0f);
}
