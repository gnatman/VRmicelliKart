#ifndef VR_MODE_H
#define VR_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief VR Mode lifecycle and state management.
 *
 * Phase 3: Desktop-only first-person camera on the kart.
 * Phase 4a+: OpenXR session, swapchains, per-eye rendering.
 */

/**
 * VR Camera Modes
 */
#define VR_CAMERA_COCKPIT 0
#define VR_CAMERA_CHASE   1
#define VR_CAMERA_HOOD    2

/**
 * @brief Check if VR mode is currently active and all preconditions are met.
 * Conditions: gVR.Enabled CVar set, single player, RACING game state.
 */
int VR_IsEnabled(void);

/**
 * @brief Initialize VR subsystem. Call once at engine startup.
 * In Phase 3 this is a stub. Phase 4a+ wires to OpenXR.
 */
void VR_Init(void);

/**
 * @brief Shut down VR subsystem. Call at engine destroy.
 */
void VR_Shutdown(void);

/**
 * @brief Called at the start of each frame to update VR pose data.
 * In Phase 3: reads mouse delta to update head yaw/pitch.
 * In Phase 4a+: calls xrWaitFrame, xrBeginFrame, xrLocateViews.
 */
void VR_PreFrame(void);

/**
 * @brief Get the current VR camera mode (cockpit/chase/hood).
 */
int VR_GetCameraMode(void);

/**
 * @brief Get the current head yaw in radians.
 */
float VR_GetHeadYaw(void);

/**
 * @brief Get the current head pitch in radians.
 */
float VR_GetHeadPitch(void);

/**
 * @brief Reset head orientation to center (recenter).
 */
void VR_Recenter(void);

#ifdef __cplusplus
}
#endif

#endif // VR_MODE_H
