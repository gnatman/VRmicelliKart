#ifndef VRMODE_H
#define VRMODE_H

#ifdef __cplusplus
#include <libultraship.h>
#endif

#include "camera.h"
#include "player_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

void VRMode_ApplyOverride(Camera* camera);
void VRMode_UpdatePlayerHead(Player* player, bool hide);

#ifdef __cplusplus
}
#endif

#endif
