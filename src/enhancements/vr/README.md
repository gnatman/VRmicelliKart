# VR Module

## Purpose
Provides an immersive first-person stereoscopic view of the game, designed for OpenXR and PC VR headsets (like PSVR2).

## How It Fits
- **VRMode:** Controls the lifecycle of the VR session.
- **VRCamera:** Overrides the native Mario Kart 64 `Camera` structure to anchor to the kart and track the player's head.
- **VRHUD & VRMenu:** Intercepts 2D rendering passes and redirects them to world-locked or head-locked `XrCompositionLayerQuad` surfaces.
- **VRComfort:** Provides vignette (for lateral Gs) and snap-turning to mitigate motion sickness.

## Dependencies
Requires the **Direct3D 11** rendering backend in `libultraship` and links against the `openxr-loader`.
