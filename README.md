# VRmicelliKart: Mario Kart 64 in Virtual Reality

[![OpenXR](https://img.shields.io/badge/OpenXR-D3D11-blue.svg)](https://www.khronos.org/openxr/)
[![Target](https://img.shields.io/badge/Target-PSVR%202%20%7C%20SteamVR-purple.svg)]()
[![Force Feedback](https://img.shields.io/badge/FFB-Direct%20Drive-orange.svg)]()
[![Telemetry](https://img.shields.io/badge/Telemetry-SimHub%20UDP-yellow.svg)]()

**VRmicelliKart** is an enhanced PC port of *Mario Kart 64* (branched from [HarbourMasters/SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) and powered by [libultraship](https://github.com/Kenix3/libultraship)) featuring native OpenXR Virtual Reality, an immersive 3D first-person cockpit, Direct Drive Force Feedback wheel support, and high-frequency motion platform telemetry.

---

## Highlights

### 🥽 Native OpenXR Virtual Reality
* **Dual-Eye 6DoF Stereoscopic Rendering:** Built on Direct3D 11 with OpenXR swapchains, targeting the Sony PlayStation VR 2 (via official PC Adapter + SteamVR), Valve Index, Meta Quest (via Link/AirLink), and compatible PCVR headsets.
* **Base Tracking Space Integration:** The game camera tracks the kart continuously in world space, while 6DoF head tracking (look-around and lean) is seamlessly composed on top.
* **Decoupled Quad-Layer HUD (Approach C):** 2D UI elements (lap counter, position, item box, minimap, character ranks) render to a dedicated 640×480 OpenXR Composition Quad Layer via explicit `G_NOOP` markers. Text and icons remain sharp, head-locked, and free of 3D distortion or ghosting.
* **3D Infinite Sky Dome:** Skyboxes render with rotation-only tracking, keeping clouds, horizons, and stars infinitely distant.
* **120Hz & 90Hz Frame Pacing:** Automatic refresh rate query (`XR_FB_display_refresh_rate`) with a delta-time simulation accumulator (`gDeltaTime`), ensuring logic runs at exactly 1.0x wall-clock speed regardless of display refresh rate.
* **Zero-Overhead Spectator Mirror:** PC monitor spectator view composited from the left-eye texture with no extra draw passes and non-blocking present.

### 🏎️ First-Person 3D Cockpit
* **Driver's Seat Immersion:** True first-person cockpit mode placing the player in the kart seat.
* **Live Geometry Sliders:** Real-time PortMenu controls for driver eye position (X/Y/Z), kart body visibility, and steering wheel alignment.
* **Dynamic Cutscene Support:** Awards ceremonies and intro sequences automatically transition to proper cinematic cameras.
* **Powerslide Effects:** Restored multi-stage tire sparks visible from the driver's perspective.

### 🛞 Direct Drive Racing Wheel & Force Feedback
* **DirectInput FFB Engine:** Low-latency force feedback driver designed for Direct Drive wheelbases (Thrustmaster T818, Fanatec, Moza, Simucube, Logitech) and belt/gear wheels.
* **Hardware Soft-Lock:** Configurable rotation limits (e.g. 360°, 540°, 900°) with realistic end-stop resistance forces.
* **Analog Steering Physics Override:** Replaces digital steering inputs with continuous, full-authority steering curves.
* **Dynamic Feedback Sensations:** Synthesizes road friction, curb vibration, collision jolts, tire scrub, and gear-shift haptic impulses.
* **Wheel Item Launch:** Throw items forward or backward using wheel paddle shifters or buttons, with directional HUD indicators.

### 📡 SimHub UDP Telemetry v2
* **Motion Platform & Transducer Streaming:** Background UDP broadcaster transmitting 140-byte binary packets to SimHub at 60 Hz.
* **Complete Motion Telemetry:** Real-time pitch, roll, yaw, angular velocities, surge/sway/heave G-forces, suspension travel, and wheel slip.
* **Rig Safety & Filtering:** Exponential Moving Average (EMA) filtering, configurable speed scaling, and pre-race launch jolt suppression to prevent physical rig boundary collisions.

---

## Quick Start

VRmicelliKart does not include copyrighted assets. You must provide a legally dumped copy of the game.

### 1. Verify your ROM dump
Only the **US ROM** is supported. Verify with an SHA-1 checksum:
```
SHA-1: 579C48E211AE952530FFC8738709F078D5DD215E
```

### 2. Verify ROM format
The ROM must be in `.z64` format (big-endian). If it is `.n64`, convert it to `.z64` using a byte-swapping utility.

### 3. Build or Extract
On Windows, run asset extraction to generate the required `.o2r` archives:
```powershell
cmake --build build --target ExtractAssets --config Release
```
Place your ROM in the game root directory when prompted.

### 4. Play
Launch `build\Release\Spaghettify.exe`. Make sure SteamVR is running if playing in VR mode!

---

## In-Game Configuration (`Esc` or Menubar)

Press **Esc** or click the menubar at the top of the spectator window to open the ImGui settings:

* **Settings -> VR:** Enable VR mode, toggle First-Person Cockpit, adjust IPD scale, world scale, and HUD distance/width in meters.
* **Settings -> Cockpit:** Tune driver eye position (X, Y, Z offsets), toggle kart body drawing, and configure spark visibility.
* **Settings -> Wheel & FFB:** Select wheel device, set steering lock degrees (soft-lock), calibrate throttle/brake axes, adjust FFB strength, and map item launch buttons.
* **Settings -> Telemetry:** Toggle SimHub UDP broadcast, set destination IP/port (default `127.0.0.1:20777`), and tune motion speed scaling.

---

## Controls

### Default Keyboard & Gamepad
| Action | N64 Button | Keyboard | SDL Gamepad | Racing Wheel |
|---|---|---|---|---|
| Accelerate | A | Shift | A | Throttle Pedal |
| Brake / Reverse | B | Ctrl | X | Brake Pedal |
| Use Item | Z | Z | LT | Paddle / Button |
| Steer | Stick Left/Right | Left / Right | Left Stick | Steering Wheel |
| Hop / Drift | R | Space | RB | Wheel Button |
| Settings Menu | — | Esc | Start | Menu Button |

---

## Building from Source (Windows)

### Requirements
* Visual Studio 2022 Community (Desktop development with C++, MSVC v143).
* Windows 10/11 SDK.
* CMake 3.24+.
* SteamVR installed and configured as the default OpenXR runtime.

### Build Commands
```powershell
# Configure CMake with VR enabled
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DENABLE_VR=ON

# Compile Release binary
cmake --build build --config Release

# Extract Assets (on first run)
cmake --build build --target ExtractAssets --config Release
```

The output executable is located at:
```
build\Release\Spaghettify.exe
```

---

## Modding

Custom tracks and assets are stored in `.o2r` or `.zip` files within the `mods/` directory.
*(Note: Legacy `.otr` archives from other projects are not supported in SpaghettiKart/VRmicelliKart.)*

---

## Credits

* **HarbourMasters:** [SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) base.
* **Kenix3:** [libultraship](https://github.com/Kenix3/libultraship) engine.
* **VR MK 64 Contributors:** OpenXR VR implementation, 3D Cockpit, Direct Drive FFB, SimHub telemetry, and 120Hz frame pacing.
