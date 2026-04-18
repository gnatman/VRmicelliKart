# Project Architecture

## Repository Roles

- **VRmicelliKart/**: The Mario Kart 64 game layer (based on SpaghettiKart). Contains game logic, camera systems, and game-specific feature implementations (VR camera, wheel tuning, telemetry packets).
- **libultraship/**: The runtime layer that replaces the N64 RCP. Handles rendering backends (D3D11/OpenGL), input (SDL2), and core VR/FFB infrastructure.

## Key Architecture Decisions

| Decision | Choice | Why |
|---|---|---|
| Rendering backend for VR | **Direct3D 11** (existing `gfx_direct3d11` in libultraship) | OpenXR `XR_KHR_D3D11_enable` is universally supported; PSVR2-on-PC requires SteamVR OpenXR which is best-tuned for D3D. GL/Metal stay available for non-VR. |
| VR API | **OpenXR** via `openxr-loader` from vcpkg | Apache 2.0, cross-runtime (SteamVR, Oculus, WMR, Varjo). |
| Wheel input | SDL2 via libultraship's existing `ControlDeck` pipeline | No reason to duplicate; we just add a new `WheelAxisMapping`. |
| Wheel FFB | **DirectInput8** (separate module) | SDL2 haptic is buggy on T300; DI gives full access to constant/spring/damper/periodic. Same device can be held by SDL (for reads) and DI (for FFB writes) simultaneously on Windows. |
| Telemetry packet | **Project CARS 2 UDP v2** (default) | PC2 is SimHub's richest-supported format. A minimum-viable PC2 packet is sufficient for motion output. |

## Critical Integration Points

### VRmicelliKart Hooks

| Path | Role |
|---|---|
| `src/camera.c` | Camera globals (`cameras[16]`, `camera1..4`, `gFreecamCamera`). Hook point for VR camera overrides. |
| `src/player_controller.c` | Where stick input is consumed. Injection point for wheel input. |
| `src/port/Engine.cpp` | Main game loop (`StartFrame`, `ProcessFrame`). Driving point for VR frame lifecycle and telemetry updates. |
| `src/port/interpolation/FrameInterpolation.cpp` | Upsamples 30Hz game ticks to display rate. Critical for VR comfort. |

### libultraship Hooks

| Path | Role |
|---|---|
| `gfx_direct3d11.cpp` | D3D11 backend. Provides `ID3D11Device*` for OpenXR. |
| `Fast3dWindow.cpp` | Core window/rendering lifecycle. |
| `interpreter.cpp` | Fast3D GBI interpreter. Tracks render targets; needs per-eye framebuffer support. |
| `ControlDeck.h` | Input management. Host for new wheel axis mappings. |
