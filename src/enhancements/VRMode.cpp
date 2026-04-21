#include "VRMode.h"
#include <libultraship/libultraship.h>
#include <ship/window/Window.h>
#include <spdlog/spdlog.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C" {
#include "camera.h"
#include "player_controller.h"
#include "racing/math_util.h"
#include "main.h"
#include "defines.h"
}

void VRMode_ApplyOverride(Camera* camera) {
    if (!CVarGetInteger("gVRMode", 0)) {
        CVarSetInteger("gVRCinemaMode", 0);
        return;
    }

    // Detect non-racing states for Cinema Mode
    bool racing = (gGamestate == RACING);
    CVarSetInteger("gVRCinemaMode", !racing);

    auto window = Ship::Context::GetInstance()->GetWindow();
    if (window == nullptr) return;

    Ship::VRPose pose = window->GetVRPose();

    float worldScale = CVarGetFloat("gVRWorldScale", 100.0f);
    
    // Get active player for anchoring
    Player* player = gPlayerOne;
    if (player == nullptr) return;

    // 1. Construct Parent Matrix (Kart)
    Mat4 parentMtx;
    mtxf_identity(parentMtx);
    
    // Copy orientation from kart (Mat3 to Mat4)
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            parentMtx[i][j] = player->orientationMatrix[i][j];
        }
    }
    
    parentMtx[3][0] = player->pos[0];
    parentMtx[3][1] = player->pos[1] + 5.0f; // Shift to driver head height
    parentMtx[3][2] = player->pos[2];

    if (!CVarGetInteger("gVRCockpitView", 0)) {
        // Chase view offset in kart-local space
        float chaseDist = 60.0f;
        float chaseHeight = 25.0f;
        
        // Translate the parent frame backwards and up relative to the kart
        parentMtx[3][0] -= parentMtx[2][0] * chaseDist;
        parentMtx[3][1] += chaseHeight;
        parentMtx[3][2] -= parentMtx[2][2] * chaseDist;
    }

    // 2. Construct Child Matrix (HMD)
    Mat4 childMtx;
    Mat4 tempMtx, rotMtx;
    mtxf_identity(childMtx);
    
    // Translation (HMD meters -> World Scale)
    float offsetX = CVarGetFloat("gVROffsetX", 0.0f);
    float offsetY = CVarGetFloat("gVROffsetY", 0.0f);
    float offsetZ = CVarGetFloat("gVROffsetZ", 0.0f);
    
    childMtx[3][0] = (pose.pos[0] * worldScale) + offsetX;
    childMtx[3][1] = (pose.pos[1] * worldScale) + offsetY;
    childMtx[3][2] = (pose.pos[2] * worldScale) + offsetZ;
    
    // Rotation (VR is CCW, N64 is CW - use matrix rotation utilities)
    mtxf_identity(rotMtx);
    
    // Convert radians to N64 s16 angles
    s16 pitch = (s16)(-pose.rot[0] * 32768.0f / M_PI); // Invert pitch if needed
    s16 yaw   = (s16)(-pose.rot[1] * 32768.0f / M_PI); // Negate yaw for CCW -> CW
    s16 roll  = (s16)(pose.rot[2] * 32768.0f / M_PI);
    
    // Combine HMD rotations
    mtxf_rotate_y(tempMtx, yaw);
    mtxf_multiplication(rotMtx, rotMtx, tempMtx);
    mtxf_rotate_x(tempMtx, pitch);
    mtxf_multiplication(rotMtx, rotMtx, tempMtx);
    mtxf_s16_rotate_z(tempMtx, roll);
    mtxf_multiplication(rotMtx, rotMtx, tempMtx);
    
    // Merge rotation into child matrix
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            childMtx[i][j] = rotMtx[i][j];
        }
    }

    // 3. Parent-Child Multiplication (Final Camera Matrix)
    Mat4 finalMtx;
    mtxf_multiplication(finalMtx, childMtx, parentMtx); // child * parent for row-major local->world

    // 4. Update Camera Struct
    // Position
    camera->pos[0] = finalMtx[3][0];
    camera->pos[1] = finalMtx[3][1];
    camera->pos[2] = finalMtx[3][2];
    
    // Extract Vectors for LookAt
    Vec3f forward; // Row 2 is typically forward/backward
    forward[0] = finalMtx[2][0];
    forward[1] = finalMtx[2][1];
    forward[2] = finalMtx[2][2];
    
    float lookDist = 100.0f;
    camera->lookAt[0] = camera->pos[0] + forward[0] * lookDist;
    camera->lookAt[1] = camera->pos[1] + forward[1] * lookDist;
    camera->lookAt[2] = camera->pos[2] + forward[2] * lookDist;
    
    // Up Vector (Row 1 is Up)
    camera->up[0] = finalMtx[1][0];
    camera->up[1] = finalMtx[1][1];
    camera->up[2] = finalMtx[1][2];

    // Update internal rotation s16 values for consistency
    float dx = camera->lookAt[0] - camera->pos[0];
    float dy = camera->lookAt[1] - camera->pos[1];
    float dz = camera->lookAt[2] - camera->pos[2];
    camera->rot[1] = (s16)atan2s(dx, dz);
    camera->rot[0] = (s16)atan2s(sqrtf(dx * dx + dz * dz), dy);
    camera->rot[2] = 0; // Roll handled by Up vector
    
    // Diagnostics
    if (CVarGetInteger("gVRDebugCamera", 0)) {
        static uint32_t frameCounter = 0;
        if (frameCounter++ % 60 == 0) {
            SPDLOG_INFO("VR Matrix Diag: ParentPos({:.1f}, {:.1f}, {:.1f}) FinalPos({:.1f}, {:.1f}, {:.1f})",
                parentMtx[3][0], parentMtx[3][1], parentMtx[3][2],
                camera->pos[0], camera->pos[1], camera->pos[2]);
            SPDLOG_INFO("VR Matrix Diag: HMD Euler Deg({:.1f}, {:.1f}, {:.1f})",
                pose.rot[0] * 180 / M_PI, pose.rot[1] * 180 / M_PI, pose.rot[2] * 180 / M_PI);
        }
    }

    VRMode_UpdatePlayerHead(player, CVarGetInteger("gVRCockpitView", 0));
}

void VRMode_UpdatePlayerHead(Player* player, bool hide) {
    // No-op for now, we'll handle hiding in render_player.c
    if (player == nullptr) return;
}
