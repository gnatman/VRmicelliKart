#include "VRMode.h"
#include <libultraship/libultraship.h>
#include <ship/window/Window.h>
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
    
    // VR Offset (for adjusting seat position)
    float offsetX = CVarGetFloat("gVROffsetX", 0.0f);
    float offsetY = CVarGetFloat("gVROffsetY", 0.0f);
    float offsetZ = CVarGetFloat("gVROffsetZ", 0.0f);

    // Apply HMD pose relative to current camera position
    // We add VRPose pos (scaled) to the camera's base position.
    camera->pos[0] += (pose.pos[0] * worldScale) + offsetX;
    camera->pos[1] += (pose.pos[1] * worldScale) + offsetY;
    camera->pos[2] += (pose.pos[2] * worldScale) + offsetZ;

    // Head tracking rotation
    float pitch = pose.rot[0];
    float yaw = pose.rot[1] + M_PI;
    float roll = pose.rot[2];

    // Update rotation s16 values for internal consistency
    // MK64 Camera rot is Vec3s (s16).
    // rot[0] = roll, rot[1] = yaw, rot[2] = pitch (based on camera.h comments)
    camera->rot[0] = (s16)(roll * 32768.0f / M_PI);
    camera->rot[1] = (s16)(yaw * 32768.0f / M_PI);
    camera->rot[2] = (s16)(pitch * 32768.0f / M_PI);

    // Update lookAt point based on head orientation
    float dist = 100.0f;
    camera->lookAt[0] = camera->pos[0] + dist * std::sin(yaw) * std::cos(pitch);
    camera->lookAt[1] = camera->pos[1] + dist * std::sin(pitch);
    camera->lookAt[2] = camera->pos[2] + dist * std::cos(yaw) * std::cos(pitch);

    // Ensure Up vector is updated (MK64 usually assumes Up is 0,1,0)
    camera->up[0] = -std::sin(roll) * std::cos(yaw);
    camera->up[1] = std::cos(roll);
    camera->up[2] = std::sin(roll) * std::sin(yaw);
}

void VRMode_UpdatePlayerHead(Player* player, bool hide) {
    if (player == nullptr) return;

    // Tracking original size per player to restore it
    static float originalSize[8] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    
    // Calculate playerId by pointer arithmetic from gPlayerOne
    int playerId = player - gPlayerOne;
    if (playerId < 0 || playerId >= 8) return;

    if (hide) {
        if (originalSize[playerId] < 0) {
            originalSize[playerId] = player->size;
        }
        player->size = 0.0f;
    } else {
        if (originalSize[playerId] >= 0) {
            player->size = originalSize[playerId];
            originalSize[playerId] = -1.0f;
        }
    }
}
