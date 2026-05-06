#ifndef VR_CAMERA_H
#define VR_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "camera.h"

/**
 * @brief Override the camera for VR first-person view.
 *
 * Positions the camera at the player's head position on the kart,
 * with orientation driven by mouse (Phase 3) or HMD tracking (Phase 4b+).
 *
 * @param camera  The game camera to override.
 * @param player  The player whose kart we are riding.
 * @param eye     Eye index: 0 = left/mono, 1 = right. (Phase 3: always 0)
 */
void VR_OverrideCamera(Camera* camera, Player* player, int eye);

#ifdef __cplusplus
}
#endif

#endif // VR_CAMERA_H
