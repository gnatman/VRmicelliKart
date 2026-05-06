# VR Enhancement Module

## Overview

This module adds immersive first-person VR support to VRmicelliKart using OpenXR with the D3D11 rendering backend.

## Files

| File | Purpose |
|---|---|
| `VRMode.h/.cpp` | VR lifecycle management (init/shutdown/preframe), state queries |
| `VRCamera.h/.cpp` | Camera override logic — positions camera at driver's eye level |

## Phases

- **Phase 3 (Current)**: Desktop-only first-person camera. Mouse right-click-drag rotates head.
- **Phase 4a**: OpenXR scaffold, swapchains, stereo rendering.
- **Phase 4b**: Per-eye projection with asymmetric frustums from HMD.
- **Phase 5**: HUD quad, menu quad, comfort features (vignette, snap-turn).

## CVars

| CVar | Type | Default | Description |
|---|---|---|---|
| `gVR.Enabled` | int | 0 | Toggle VR mode |
| `gVR.CameraMode` | int | 0 | 0=Cockpit, 1=Chase, 2=Hood |
| `gVR.FOV` | float | 90.0 | Field of view override (Phase 3 only) |

## Usage

1. Enable VR mode via the SpaghettiGui menu: **Enhancements → VR → Enable VR**
2. Start a single-player race
3. Hold right mouse button and drag to look around
4. Camera automatically follows the kart

## Preconditions

VR mode activates only when:
- `gVR.Enabled` CVar is set to 1
- Game is in single-player mode (`SCREEN_MODE_1P`)
- Game state is `RACING` (value 4)
