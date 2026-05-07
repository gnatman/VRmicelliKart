# Racing Wheel & Force Feedback

## Overview
VRmicelliKart supports steering wheels with force feedback (FFB). Wheel axis mapping is handled via SDL2 natively within libultraship, while FFB effects bypass SDL2's haptic layer in favor of Microsoft DirectInput8 to support advanced periodic and constant forces.

## Tested Devices
- Thrustmaster T300 / TX / TMX / T248 family (belt-driven, 1080° DOR).

## Force Feedback Effects
- **Centering Spring:** Velocity and steering angle-dependent.
- **Surface Rumble:** Periodic sine/triangle waves changing frequency based on speed and amplitude based on terrain (`surfaceType`).
- **Bump/Hop:** One-shot impacts triggered by `kartHopVelocity`.
- **Collision Pulse:** Strong constant force upon wall/actor collisions.

## CVars & Tuning
Configure via the "Wheel" ImGui menu:

| CVar | Default | Purpose |
|---|---|---|
| `gWheel.Enabled` | 0 | Master switch |
| `gWheel.DegreesOfRotation` | 1080 | Wheel hardware rotation range (e.g., 900 for G29) |
| `gWheel.Linearity` | 0.85 | Steering curve (<1 precise center, >1 twitchy) |
| `gWheel.Deadzone` | 0.02 | Center deadzone |
| `gWheel.FFBStrength` | 0.8 | Master FFB multiplier |
| `gWheel.FFBCenteringSpring`| 0.6 | Spring scale |
| `gWheel.FFBDamper` | 0.3 | Damper scale |
| `gWheel.FFBRumbleScale` | 0.5 | Surface rumble scale |
| `gWheel.FFBCollisionScale` | 1.0 | Collision pulse scale |

## Axis Binding
Because some wheels (like the T300) split pedals across unusual axes (Y, Z, Rz), they must be mapped manually in the UI using `gWheel.ThrottleAxis`, `gWheel.BrakeAxis`, and `gWheel.ClutchAxis`.
