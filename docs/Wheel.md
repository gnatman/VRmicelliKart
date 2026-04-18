# Racing Wheel & Force-Feedback Plan

## Overview
Support for racing wheels via SDL2 for input and DirectInput8 for force-feedback.

## CVars

| CVar | Type | Default (T300 preset) | Purpose |
|---|---|---|---|
| `gWheel.Enabled` | int 0/1 | 0 | Master switch |
| `gWheel.DeviceGUID` | string | "" | SDL joystick GUID of active wheel |
| `gWheel.DegreesOfRotation` | int | 1080 | T300 default; 900 for G29/G923 |
| `gWheel.Linearity` | float | 0.85 | <1 = more precise center, >1 = twitchier |
| `gWheel.Deadzone` | float | 0.02 | Center dead zone |
| `gWheel.ThrottleAxis` | int | 1 (Y) | DI axis index |
| `gWheel.BrakeAxis` | int | 2 (Z) | DI axis index |
| `gWheel.ClutchAxis` | int | 5 (Rz) | DI axis index |
| `gWheel.FFBStrength` | float | 0.8 | Master FFB multiplier |
| `gWheel.FFBCenteringSpring` | float | 0.6 | Scaled by speed |
| `gWheel.FFBDamper` | float | 0.3 | |
| `gWheel.FFBRumbleScale` | float | 0.5 | |
| `gWheel.FFBCollisionScale` | float | 1.0 | |
| `gWheel.FFBInvert` | int 0/1 | 0 | |

## FFB Effects

- **Centering spring**: Proportional to steering angle and speed.
- **Damper**: Proportional to steering velocity.
- **Surface rumble**: Periodic effect driven by speed and surface type.
- **Bump one-shot**: Triggered on kart hop/landing.
- **Collision pulse**: Strong constant force on collision.

## Hardware Support
- Primary Target: **Thrustmaster T300 / TX / TMX / T248 family**.
- Force-Feedback: **DirectInput8** (Windows only).
