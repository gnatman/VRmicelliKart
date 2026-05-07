# VRmicelliKart Architecture

## Overview
VRmicelliKart extends Mario Kart 64 (via SpaghettiKart and libultraship) with three major enhancements: VR, Wheel input/FFB, and Telemetry.

## Implementation Status (Phase 6)
- **VR:** OpenXR + D3D11 backend. Per-eye projection/view matrix overrides in `guPerspectiveF`.
- **Wheel:** Custom SDL axis mapping with power-curve linearity.
- **FFB:** DirectInput8 constant/spring/periodic effects.
- **Telemetry:** Project CARS 2 UDP v2 broadcasting.

## Critical Files and Hook Points
- `SpaghettiKart/src/os/guPerspectiveF.c`: VR asymmetric frustum injection.
- `SpaghettiKart/src/enhancements/vr/VRMode.cpp`: Camera anchor and comfort logic.
- `SpaghettiKart/src/port/SpaghettiControlDeck.cpp`: Wheel input injection.
- `libultraship/src/vr/VRSession.cpp`: OpenXR lifecycle and render loop.
- `libultraship/src/ffb/DirectInputFFB.cpp`: Win32 FFB implementation.
