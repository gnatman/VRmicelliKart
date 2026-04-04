#include "VRCamera.h"
#include "port/vr/VRManager.h"
#include "port/interpolation/FrameInterpolation.h"
#include <libultraship.h>
#include <libultra/gbi.h>

VRCamera::VRCamera(FVector pos, s16 rot, u32 mode) : GameCamera(pos, rot, mode) {
}

void VRCamera::Tick() {
    GameCamera::Tick();
    // In a full implementation, we would apply HMD tracking to the camera position/rotation here
    VR_Update();
}

void VRCamera::SetViewProjection() {
    if (!VR_IsActive()) {
        GameCamera::SetViewProjection();
        return;
    }

    // VR rendering is handled in the GameEngine loop by calling the interpreter twice.
    // This method is called during display list generation.
    // For VR, we want to emit a "placeholder" or indicate that we're in VR mode.
    
    // We'll still call the base implementation to ensure the display list has some valid matrices,
    // but the actual eye matrices will be injected during GameEngine::ProcessGfxCommands.
    GameCamera::SetViewProjection();
}
