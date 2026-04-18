# VR Feature Plan

## Overview
Stereoscopic VR mode implemented via OpenXR and Direct3D 11.

## CVars

| CVar | Type | Default | Purpose |
|---|---|---|---|
| `gVR.Enabled` | int 0/1 | 0 | Master switch (requires D3D11 backend + restart) |
| `gVR.IPDScale` | float | 1.0 | Scales eye-separation relative to world |
| `gVR.WorldScale` | float | 1.0 | Scales kart/world relative to player (cockpit feel) |
| `gVR.CameraMode` | int | 0 | 0=cockpit, 1=chase, 2=hood |
| `gVR.HUDDistance` | float (m) | 1.5 | World-locked HUD depth |
| `gVR.HUDScale` | float | 1.0 | HUD quad size |
| `gVR.MenuLockMode` | int | 1 | 0=world, 1=head, 2=wrist |
| `gVR.SnapTurn` | int 0/1 | 0 | Comfort option |
| `gVR.SnapTurnDegrees` | int | 30 | 15 / 30 / 45 |
| `gVR.Vignette` | float 0–1 | 0.5 | Lateral-G comfort vignette |
| `gVR.MirrorEye` | int | 0 | Desktop mirror: 0=left, 1=right, 2=both |

## Feature Areas

### 1. OpenXR Lifecycle
Handled in `libultraship` via `VRSession`. Manages instance, session, and swapchains.

### 2. Camera Overrides
Handled in `VRmicelliKart` via `VRCamera.cpp`. Overrides N64 camera position/rotation with head-tracked pose.

### 3. VR HUD & Menus
2D HUD is rendered as a world-locked quad. Menus are rendered into a head-locked quad in non-race states.

### 4. Comfort Features
Includes vignette shader driven by lateral G-forces and snap-turn.

## Hardware Support
- Primary Target: **PSVR2** via Sony PC Adapter + SteamVR OpenXR runtime.
- Reference Space: `XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR`.
