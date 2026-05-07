#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "common_structs.h"
#include "camera.h"

// Is the VR camera currently enabled?
bool VR_IsEnabled();

// Applies the first-person camera override for the given camera index
void VR_OverrideCamera(Camera* camera, Player* player, int eye);

// Returns the vignette intensity (0.0 to 1.0) based on lateral Gs
float VR_GetVignetteIntensity(Player* player);

#ifdef __cplusplus
}
#endif
