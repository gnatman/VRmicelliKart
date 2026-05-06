#include <libultraship.h>

extern "C" {
#include <macros.h>
#include <defines.h>
#include <common_structs.h>
#include <libc/math.h>
#include "main.h"
#include "camera.h"
#include "racing/math_util.h"
#include "racing/collision.h"
#include "enhancements/vr/VRMode.h"
#include "enhancements/vr/VRCamera.h"
}

#include <cmath>

/**
 * VR Camera Implementation — Matrix Anchoring
 *
 * This version anchors the camera to the kart's physical chassis using its
 * orientationMatrix. This ensures the view correctly inherits all 3D rotations
 * of the vehicle (pitch, roll, yaw) while allowing independent head look.
 */

// Height offset above the kart position for the driver's eye level
static const float kCockpitHeightOffset = 8.0f;
static const float kCockpitForwardOffset = 0.0f;

void VR_OverrideCamera(Camera* camera, Player* player, int eye) {
    (void)eye; 

    if (player == NULL || camera == NULL) {
        return;
    }

    // 1. Get VR Head Pose Data
    float headYaw = VR_GetHeadYaw();
    float headPitch = VR_GetHeadPitch();
    float headRoll = VR_GetHeadRoll();
    float headPos[3];
    VR_GetHeadPos(&headPos[0], &headPos[1], &headPos[2]);
    
    float worldScale = CVarGetFloat("gVR.WorldScale", 50.0f);

    // 2. Construct Kart Anchor Matrix (Parent)
    // player->orientationMatrix is World-to-Local (for rendering),
    // so we transpose it to get Local-to-World for our anchor.
    Mat4 kartAnchor;
    mtxf_identity(kartAnchor);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            kartAnchor[i][j] = player->orientationMatrix[j][i];
        }
    }
    
    // Set kart position (driver seat)
    kartAnchor[3][0] = player->pos[0];
    kartAnchor[3][1] = player->pos[1] + kCockpitHeightOffset;
    kartAnchor[3][2] = player->pos[2];

    // 3. Construct HMD Local Matrix (Child)
    Mat4 hmdLocal, tempMtx;
    mtxf_identity(hmdLocal);
    
    // Apply HMD Position (Positional Tracking)
    hmdLocal[3][0] = headPos[0] * worldScale;
    hmdLocal[3][1] = headPos[1] * worldScale;
    hmdLocal[3][2] = headPos[2] * worldScale;
    
    // Apply HMD Rotation (Head Tracking)
    // N64 rotations are s16 angles (0-65535)
    s16 sPitch = (s16)(headPitch * 32768.0f / M_PI);
    s16 sYaw   = (s16)(headYaw * 32768.0f / M_PI);
    s16 sRoll  = (s16)(headRoll * 32768.0f / M_PI);
    
    // Order: Y (Yaw) -> X (Pitch) -> Z (Roll)
    mtxf_rotate_y(tempMtx, sYaw);
    mtxf_multiplication(hmdLocal, hmdLocal, tempMtx);
    mtxf_rotate_x(tempMtx, sPitch);
    mtxf_multiplication(hmdLocal, hmdLocal, tempMtx);
    mtxf_s16_rotate_z(tempMtx, sRoll);
    mtxf_multiplication(hmdLocal, hmdLocal, tempMtx);

    // 4. Multiply HMD * Kart to get Final Camera Matrix
    Mat4 finalMtx;
    mtxf_multiplication(finalMtx, hmdLocal, kartAnchor);

    // 5. Update N64 Camera Struct
    // Position (Row 3)
    camera->pos[0] = finalMtx[3][0];
    camera->pos[1] = finalMtx[3][1];
    camera->pos[2] = finalMtx[3][2];
    
    // Update camera collision/segment data so the engine knows where we are.
    // This fixes the "disappearing road" issue caused by track segment culling.
    check_bounding_collision(&camera->collision, 10.0f, camera->pos[0], camera->pos[1], camera->pos[2]);

    // Extract Forward Vector (Row 2) for LookAt
    float lookDistance = 50.0f;
    camera->lookAt[0] = camera->pos[0] + finalMtx[2][0] * lookDistance;
    camera->lookAt[1] = camera->pos[1] + finalMtx[2][1] * lookDistance;
    camera->lookAt[2] = camera->pos[2] + finalMtx[2][2] * lookDistance;
    
    // Extract Up Vector (Row 1)
    camera->up[0] = finalMtx[1][0];
    camera->up[1] = finalMtx[1][1];
    camera->up[2] = finalMtx[1][2];

    // Field of View
    camera->fieldOfView = CVarGetFloat("gVR.FOV", 90.0f);
    
    // Internal rotation values for legacy HUD/Logic (Yaw only)
    camera->rot[1] = (s16)atan2s(finalMtx[2][0], finalMtx[2][2]);
    camera->rot[0] = 0;
    camera->rot[2] = 0;
}
