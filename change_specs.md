# VRmicelliKart — VR + Racing Wheel + Motion-Sim Telemetry

## Context

The goal is to modify the source code for **Mario Kart 64** so it supports:

1. **Immersive first-person VR mode** (optional, preserving 2D mode)
2. **Racing wheel controls** with force-feedback and rebindable buttons
3. **Motion-simulator telemetry** (UDP output for SimHub/SimTools/Yaw/DOF Reality)

The hardest part is going to be the VR part of the project since the way that the Nintendo 64 renders graphics is different from some modern implementations.  Especially difficult is going to be handling the seperation between the 3D world and the 2D menus and HUD elements while racing.  Make sure that you come up with a comprehensive plan, and add plenty of debugging and documentation throughout the process so that we can troubleshoot any issues that may arise.  Another important consideration is going to be understanding the interactions between the kart's velocity vector as the user is turning the wheel, the VR headset's orientation as the user turns their head, and how the camera and player movement work together to create a sense of immersion.  Take a look at how other VR racing games handle this, and come up with a plan that you think will work well for this project.

You may need to research some of these topics by searching the web if you are not confident in your own understanding or ability to perform these modifications.

There is a freecam modification already built into the game, but you may not want to rely on it because it has collisions disabled in order to be able to freely explore the world, but we need collisions in order for the driving functions to behave properly.

The work spans two local repositories that live side-by-side:

- `VRmicelliKart/` — the MK64 game layer (based on SpaghettiKart, itself based on the `mk64` decompilation). Builds `Spaghettify.exe`. C code with a C++ `src/port/` bridge layer.
- `libultraship/` — the runtime that replaces the N64 RCP (Reality Co-Processor). Fast3D GBI translator → OpenGL / Direct3D 11 / Metal. Handles SDL2 input, ImGui, CVar config, ImGui windows, threading, audio.

The codebase is in exceptionally good shape for this project:
- **`FrameInterpolation`** (`src/port/interpolation/`) already upsamples the 30 Hz native tick to display rate — this is a critical enabler for VR comfort.
- A **CVar + ImGui menu** infrastructure is already wired up (`src/port/SpaghettiGui.cpp`), so every toggle/tuning knob can surface naturally.
- libultraship's Fast3D has **per-framebuffer render targets** (`CreateFramebuffer` / `StartDrawToFramebuffer`) — the exact primitive needed for per-eye stereo rendering.

This plan produces a thoroughly documented, well-commented project that a future contributor can continue without prior context.

## User-confirmed parameters

| Parameter | Value |
|---|---|
| Target OS | **Windows only** (DirectInput FFB, D3D11-backed OpenXR) |
| Primary HMD | **PSVR2** via Sony PC Adapter + SteamVR OpenXR runtime |
| Racing wheel | **Thrustmaster T300 / TX / TMX / T248 family** — belt-driven, 1080° DOR default, 8-bit DI FFB |
| Motion-rig software | **SimHub** — telemetry format defaults to **Project CARS 2 UDP** |

## Architecture decisions

| Decision | Choice | Why |
|---|---|---|
| Rendering backend for VR | **Direct3D 11** (existing `gfx_direct3d11` in libultraship) | OpenXR `XR_KHR_D3D11_enable` is universally supported; PSVR2-on-PC requires SteamVR OpenXR which is best-tuned for D3D. GL/Metal stay available for non-VR. |
| VR API | **OpenXR** via `openxr-loader` from vcpkg | Apache 2.0, cross-runtime (SteamVR, Oculus, WMR, Varjo). |
| Wheel input | SDL2 via libultraship's existing `ControlDeck` pipeline | No reason to duplicate; we just add a new `WheelAxisMapping`. |
| Wheel FFB | **DirectInput8** (separate module) | SDL2 haptic is buggy on T300; DI gives full access to constant/spring/damper/periodic. Same device can be held by SDL (for reads) and DI (for FFB writes) simultaneously on Windows. |
| Telemetry packet | **Project CARS 2 UDP v2** (default), DiRT Rally 2.0 optional | PC2 is SimHub's richest-supported format. A minimum-viable PC2 packet (1 participant + 1 telemetry at 60 Hz) is sufficient for motion output. |
| OpenXR reference space | `XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR` (falls back to `STAGE` then `LOCAL`) | Seated go-kart play; floor reference is natural. |

## Critical files and hook points (verified via exploration)

### VRmicelliKart — game layer

| Path | Role |
|---|---|
| `src/camera.h`, `src/camera.c` | `Camera` struct (pos/lookAt/up/rot/fov/perspectiveMatrix/lookAtMatrix); globals `cameras[16]`, `camera1..4`, `gFreecamCamera` |
| `include/common_structs.h` | `Player` struct with `pos`, `oldPos`, `velocity`, `rotation` (Vec3s yaw/pitch/roll), `currentSpeed`, `boostPower`, `boostTimer`, `kartHopVelocity`, `driftDuration`, `driftState`, `surfaceType`, `tyres[4]`, `orientationMatrix` |
| `src/player_controller.c` ~L4475 | Where `controller->rawStickX` is consumed for steering — wheel injection point |
| `src/enhancements/freecam/` | **Reference pattern** for toggleable camera modes — mirror its shape for VR |
| `src/os/guPerspectiveF.c`, `src/os/guLookAtF.c` | N64-style projection/view math — hook for per-eye matrices |
| `src/racing/skybox_and_splitscreen.c` | Viewport setup (already supports split-screen; we bypass for VR) |
| `src/port/Engine.cpp`, `src/port/Engine.h` | `GameEngine::StartFrame()`, `RunCommands()`, `ProcessFrame()` — frame-loop hook points |
| `src/port/interpolation/FrameInterpolation.{h,cpp}` | Extend to interpolate *per display rate* for VR |
| `src/port/SpaghettiGui.cpp` | Add VR / Wheel / Telemetry menus here |
| `CMakeLists.txt` | Globs `src/**/*.c` so new subdirs auto-compile; adds `add_subdirectory(libultraship)` |

### libultraship — runtime layer

| Path | Role |
|---|---|
| `include/fast/backends/gfx_rendering_api.h` | Abstract `Fast::GfxRenderingAPI` with `SetViewport`, `CreateFramebuffer`, `StartDrawToFramebuffer`. Subclass for per-eye FB binding. |
| `src/fast/backends/gfx_direct3d11.cpp` | D3D11 backend; holds `ID3D11Device*` — hand to `XrGraphicsBindingD3D11KHR` |
| `src/fast/Fast3dWindow.cpp` | `Init`, `StartFrame`, `DrawAndRunGraphicsCommands`, `EndFrame` — library does NOT own main loop; game drives it |
| `src/fast/interpreter.cpp` | Tracks `mGameFb` / `mGameFbMsaaResolved` — we rebind per eye |
| `include/ship/controller/controldeck/ControlDeck.h` | Abstract; game subclasses and implements `WriteToPad` |
| `src/ship/controller/.../SDLRumbleMapping.cpp` | Existing SDL rumble pipeline — disable for wheel GUIDs, route via FFBManager instead |
| `include/ship/config/ConsoleVariable.h` | CVar API (`CVarGetInteger`/`SetFloat`/etc.) |
| `src/window/gui/GuiMenuBar.*` | ImGui menu bar extension points |
| `vcpkg.json` | Add `openxr-loader`; SDL2 already present |

## Feature plan

### Feature 1 — Stereoscopic VR mode (OpenXR, D3D11)

**New files (libultraship — reusable VR infrastructure):**
- `libultraship/src/vr/VRSession.{h,cpp}` — owns `XrInstance`, `XrSession`, reference `XrSpace`, per-eye `XrSwapchain`
- `libultraship/src/vr/VRSwapchain.{h,cpp}` — wraps XR swapchain images + caches D3D11 RTV/DSV
- `libultraship/src/vr/VRInput.{h,cpp}` — `XrActionSet` with suggested bindings for Sense controllers (triggers, grips, thumbsticks, system buttons) fed into ControlDeck as a synthetic controller
- `libultraship/src/vr/VRPose.h` — `struct VRPose { XrPosef head; XrPosef eyes[2]; XrFovf fov[2]; XrTime displayTime; };`
- `libultraship/src/fast/backends/gfx_direct3d11_vr.{h,cpp}` — thin extension of D3D11 backend exposing `GetDevice()` + `BindExternalRenderTarget(ID3D11Texture2D*)` so `StartDrawToFramebuffer` can target an XR swapchain image
- `libultraship/src/vr/README.md`, `libultraship/docs/VR-Runtime.md`

**New files (VRmicelliKart — game-specific wiring):**
- `src/enhancements/vr/VRMode.{h,cpp}` — lifecycle, `VR_IsEnabled()`, `VR_GetFrameData()`, ties `GameEngine` frame loop to `VRSession`
- `src/enhancements/vr/VRCamera.cpp` — per-eye camera override: `Camera.pos = playerKartAnchor + headOffsetInKartSpace + eyeOffset * IPDScale`; converts head quat → `Vec3s` yaw/pitch/roll
- `src/enhancements/vr/VRHUD.cpp` — renders 2D HUD framebuffer as world-locked quad at `gVR.HUDDistance` in front of head
- `src/enhancements/vr/VRMenu.cpp` — for title/pause/results screens, renders the 2D frame into a head-locked `XrCompositionLayerQuad`
- `src/enhancements/vr/VRComfort.cpp` — vignette shader (lateral-G driven), snap-turn, seated recenter
- `src/enhancements/vr/README.md`, `docs/VR.md`

**Existing files to modify:**
- `VRmicelliKart/src/port/Engine.cpp` — in `StartFrame` call `VRSession::WaitFrame`/`BeginFrame`; in `ProcessFrame` run per-eye loop: for each eye acquire swapchain image → bind as `mGameFb` → override camera → run render graph → release image
- `VRmicelliKart/src/camera.c` — in camera-update functions, when `CVarGetInteger("gVR.Enabled", 0)`, replace `pos` / `lookAt` / `rot` using active `VRPose` + player kart anchor
- `VRmicelliKart/src/os/guPerspectiveF.c` — if VR active, emit per-eye asymmetric frustum from `XrFovf` instead of symmetric FOV
- `VRmicelliKart/src/os/guLookAtF.c` — if VR active, compose with head pose
- `VRmicelliKart/src/racing/skybox_and_splitscreen.c` — short-circuit split-screen viewport when VR active (single viewport per eye, full swapchain)
- `VRmicelliKart/src/port/interpolation/FrameInterpolation.cpp` — ensure interpolation runs per eye-render (≥90 Hz) and covers actor transforms (not just camera); add `FrameInterpolation_ShouldInterpolateForVR()`
- `libultraship/src/fast/Fast3dWindow.cpp` — `DrawAndRunGraphicsCommands` accepts an optional external framebuffer handle
- `libultraship/src/fast/interpreter.cpp` — expose setter for `mGameFb` so XR swapchain image can be bound per-eye
- `VRmicelliKart/src/port/SpaghettiGui.cpp` — new "VR" menu
- `libultraship/CMakeLists.txt`, `VRmicelliKart/CMakeLists.txt` — `find_package(OpenXR CONFIG REQUIRED)` under `-DLUS_ENABLE_VR=ON`
- `libultraship/vcpkg.json` — add `openxr-loader`

**CVars (persisted via `Ship::Config`):**
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

**Data flow (one frame):**
1. `xrWaitFrame` → predicted `displayTime` + `shouldRender`
2. `xrLocateViews` → `XrView[2]` pose + FOV → `VRPose`
3. Game logic runs once (30 Hz tick)
4. For each eye (rendered at display rate):
   - `xrAcquireSwapchainImage` / `xrWaitSwapchainImage` → D3D11 texture
   - Bind texture as `mGameFb` via `gfx_direct3d11_vr::BindExternalRenderTarget`
   - `VR_OverrideCamera(eye)` writes head-tracked pose + eye offset into active `Camera`
   - `guPerspectiveF`/`guLookAtF` emit per-eye matrices
   - `FrameInterpolation` interpolates actor matrices using `(displayTime - lastTickTime) / tickInterval`
   - Render graph runs as normal
   - `VRHUD` composites 2D HUD framebuffer as textured quad
   - `VRComfort` draws vignette overlay
   - `xrReleaseSwapchainImage`
5. `xrEndFrame` submits layers: [projection layer, HUD quad, (if menu) menu quad]
6. Desktop mirror: blit left-eye texture to swap chain

**2D menu screens (title, results, pause):** `VRMenu` detects menu state via existing game-state enum (need to confirm its name — see Unknowns #1). In menu state, render the 2D frame once into a 2 m-wide head-locked quad at 2 m depth, submitted as `XrCompositionLayerQuad` rather than projection layer. Cockpit rendering stays suppressed.

### Feature 2 — Racing wheel controls + force-feedback

**New files (libultraship — reusable input/FFB infrastructure):**
- `src/ship/controller/controldevice/controller/mapping/wheel/WheelAxisMapping.{h,cpp}` — extends `ControllerAxisMapping`; applies deadzone, linearity curve, DOR (degrees of rotation) scaling before writing `rawStickX`
- `src/ship/controller/controldevice/controller/mapping/wheel/WheelPedalMapping.{h,cpp}` — separate throttle/brake/clutch axis handlers (T300 splits pedals across DI axes Y/Z/Rz by default)
- `src/ship/ffb/FFBDevice.h` — abstract interface: `SetConstantForce`, `SetSpring`, `SetDamper`, `PlayPeriodic`, `Stop`
- `src/ship/ffb/DirectInputFFB.{h,cpp}` — Windows DI8 implementation; enumerates, matches by `SDL_JoystickGetGUID`, creates effects
- `src/ship/ffb/NullFFB.cpp` — no-op fallback (for non-Windows or missing device)
- `src/ship/ffb/FFBManager.{h,cpp}` — singleton, owns `FFBDevice`, composites effects each frame
- `src/ship/ffb/README.md`

**New files (VRmicelliKart — game-specific):**
- `src/enhancements/wheel/WheelTuning.{h,cpp}` — applies curve: `rawStickX = sign(x) * pow(|x|, linearity) * maxDeflection` with deadzone
- `src/enhancements/wheel/FFBEffects.{h,cpp}` — maps game state → FFB effects:
  - Centering spring: `k(speed) * steeringAngle * FFBCenteringSpring`
  - Damper: `c * steeringVelocity * FFBDamper`
  - Surface rumble: sine/triangle periodic, freq = f(speed), amp = f(surfaceType)
  - Bump one-shot: triggered on `kartHopVelocity` spikes
  - Collision pulse: strong constant, triggered on collision flags
  - Boost pad: periodic triangle on `boostTimer` rising edge
- `src/enhancements/wheel/README.md`, `docs/Wheel.md`

**Existing files to modify:**
- `VRmicelliKart/src/player_controller.c` ~L4475 — route `rawStickX` through `WheelTuning_ApplyCurve(rawStickX, currentSpeed)` when wheel is active (value is already `-128..127` from ControlDeck, so this is a curve swap, not a plumbing change)
- `VRmicelliKart/src/port/Engine.cpp` — instantiate `FFBManager` in `GameEngine::Create`; call `FFBManager::Tick(player)` once per frame from `StartFrame`
- `libultraship/src/ship/controller/controldevice/controller/mapping/factories/ControllerAxisMappingFactory.cpp` — add factory case `MAPPING_TYPE_WHEEL_AXIS`
- `libultraship/src/ship/controller/controldeck/ControlDeck.h/.cpp` — add `GetControllerByGUID()` helper
- `libultraship/src/ship/controller/controldevice/controller/mapping/sdl/SDLRumbleMapping.cpp` — skip SDL rumble for devices matched as wheels (they go through DI FFB instead)
- `libultraship/CMakeLists.txt` — link `dinput8.lib dxguid.lib` on Windows
- `VRmicelliKart/src/port/SpaghettiGui.cpp` — new "Wheel" menu

**CVars:**
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

**ImGui "Wheel" menu:**
- Device dropdown (detected wheels by GUID + name)
- "Detect Rotation" button → asks user to turn wheel fully in each direction to autocalibrate DOR
- DOR / Linearity / Deadzone sliders
- Axis-assignment UI: "click throttle → press throttle pedal → assigned"
- Button rebind grid (reuses libultraship `InputEditorWindow`)
- FFB section: Master / Spring / Damper / Rumble / Collision sliders
- "Test FFB" buttons (2-second spring pulse, 2-second rumble, 200ms bump)
- Invert-FFB checkbox

### Feature 3 — UDP telemetry

**New files (VRmicelliKart — telemetry is game-specific):**
- `src/enhancements/telemetry/Telemetry.{h,cpp}` — UDP socket, worker thread, lockless SPSC ring
- `src/enhancements/telemetry/PacketPC2.{h,cpp}` — Project CARS 2 `sTelemetryData` + `sParticipantInfo` layout
- `src/enhancements/telemetry/PacketDR2.{h,cpp}` — DiRT Rally 2.0 "Extra Data 3" layout (264 bytes)
- `src/enhancements/telemetry/TelemetryCalibration.{h,cpp}` — N64 units → meters / km/h / G scaling, fake RPM/gear synthesis
- `src/enhancements/telemetry/README.md`, `docs/Telemetry.md`

**Existing files to modify:**
- `VRmicelliKart/src/port/Engine.cpp` — `Telemetry::Tick(cameras, players)` once per frame from `StartFrame`
- `VRmicelliKart/src/port/SpaghettiGui.cpp` — new "Telemetry" menu
- `VRmicelliKart/CMakeLists.txt` — link `ws2_32` on Windows if not already

**CVars:**
| CVar | Type | Default | Purpose |
|---|---|---|---|
| `gTelemetry.Enabled` | int 0/1 | 0 | Master switch |
| `gTelemetry.Format` | int | 0 | 0=PC2, 1=DR2, 2=both |
| `gTelemetry.Host` | string | "127.0.0.1" | Destination (SimHub runs on loopback by default) |
| `gTelemetry.PortPC2` | int | 5606 | PC2 default |
| `gTelemetry.PortDR2` | int | 20777 | DR2 default |
| `gTelemetry.Rate` | int Hz | 60 | 30–120 range |
| `gTelemetry.PlayerIndex` | int | 0 | Which player in `gPlayers[]` |
| `gTelemetry.SpeedScale` | float | *empirical* | N64 units → m/s |
| `gTelemetry.AccelScale` | float | *empirical* | N64 units/tick² → m/s² |
| `gTelemetry.RPMMin` | int | 900 | Fake engine idle |
| `gTelemetry.RPMMax` | int | 9500 | Fake engine redline |
| `gTelemetry.MaxGears` | int | 4 | Faked gear count |

**Data flow:**
Each game frame → `Telemetry::Tick` samples `gPlayers[gTelemetry.PlayerIndex]` + `cameras[playerIndex]`. Conversions:
- `pos × SpeedScale` → meters (x/y/z)
- `(pos - oldPos) / dt × SpeedScale` → velocity m/s
- `(vel - oldVel) / dt` → accel m/s² → G = a / 9.81
- `rotation` Vec3s → yaw/pitch/roll radians
- `currentSpeed × SpeedScale × 3.6` → km/h
- Fake RPM = `lerp(RPMMin, RPMMax, currentSpeed / maxSpeed)`; gear = `floor((currentSpeed / maxSpeed) × MaxGears) + 1`
- `boostTimer > 0` → synthetic throttle = 1.0; else `rawStickY / 127`
- `tyres[4]` → per-tyre on-ground flags; `surfaceType` → terrain code

Writes `sParticipantInfo` + `sTelemetryData` for PC2 (little-endian), hands to worker thread via SPSC ring, worker `sendto()`s at configured rate.

## Documentation layout (created as part of Phase 0)

```
VRmicelliKart/
  docs/
    PLAN.md                 # this plan, committed
    ARCHITECTURE.md         # top-down overview linking all three features
    VR.md                   # user + dev guide, PSVR2 setup notes
    Wheel.md                # T300 defaults, FFB tuning guide
    Telemetry.md            # SimHub connection walkthrough, calibration guide
    CHANGELOG.md            # phase-by-phase log
    LICENSES.md             # OpenXR (Apache 2.0), SDL2 (zlib), DirectInput (MS), our code
  src/
    enhancements/vr/README.md
    enhancements/wheel/README.md
    enhancements/telemetry/README.md
    port/README.md          # (supplement existing)

libultraship/
  docs/VR-Runtime.md        # libultraship-side VR infra docs
  src/vr/README.md
  src/ship/ffb/README.md
```

**Code documentation conventions:**
- Doxygen-compatible `/** @brief @param @return */` on every public function in new headers
- `//!` inline doc style for non-obvious struct fields
- File-header block on every new file: purpose, feature area, brief "how it fits"
- Non-obvious math gets a comment block with citation (OpenXR spec section, Codemasters forum post URL, etc.)
- `// TODO(phase-N):` markers tied to phasing plan

## Build / phasing strategy

Each phase ends with (a) working demo, (b) `CHANGELOG.md` entry, (c) docs updated, (d) atomic commit.

| Phase | Deliverable | Testable outcome |
|---|---|---|
| **0. Baseline** | Confirm both repos build; pin vcpkg baseline; create `docs/` skeleton + all per-folder READMEs; commit `PLAN.md` | `Spaghettify.exe` runs unchanged in 2D |
| **1. Telemetry** | All Feature 3 files, ImGui menu, PC2 packet, calibration UI with live readout | Launch SimHub → observe live speed / RPM / G from a practice race. *Lowest risk: no rendering or input touched.* |
| **2a. Wheel input** | `WheelAxisMapping`, `WheelPedalMapping`, Wheel ImGui menu, steering curve. No FFB yet. | Drive with T300; rebind buttons; DOR/linearity sliders work |
| **2b. Wheel FFB** | `DirectInputFFB`, `FFBManager`, `FFBEffects` with centering spring + surface rumble + collision | Feel centering spring on straights, rumble off-track, bump on hop landings |
| **3. VR camera (no stereo)** | `VRMode` "camera-only" mode: first-person head anchor on kart, toggleable. Uses desktop 2D window. Mouse/keyboard drives pitch/yaw to prove camera override works. | Cockpit view on desktop confirms camera-override pipeline *before* touching rendering |
| **4a. OpenXR scaffold + stereo** | `VRSession` lifecycle (init/loss/resume), D3D11 swapchains, per-eye loop, basic `xrEndFrame`. Mono scene rendered twice. | PSVR2 lights up, shows duplicated image; survives Alt-Tab, focus loss, device reset |
| **4b. Per-eye projection + view** | Wire `guPerspectiveF` / `guLookAtF` per-eye overrides, IPD, world scale | Proper stereoscopic 3D while driving |
| **5. Comfort + HUD + menus** | World-locked HUD quad, head-locked menu in non-race states, snap-turn, vignette, recenter | Playable race without nausea in a 20-min session; menus navigable in HMD |
| **6. Integration + tuning** | FFB ↔ telemetry ↔ VR all on simultaneously, frame-rate regression testing, final doc pass | 90/120 Hz stable on PSVR2, telemetry live, FFB live, no desyncs |

## Risk register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **30 Hz game in 90/120 Hz VR compositor** → juddery head tracking | High | High | Leverage existing `FrameInterpolation.cpp`; extend it to cover actor matrices, not just camera. Render VR per-eye at display rate while game logic stays 30 Hz. Late-latch head pose right before `xrEndFrame`. |
| R2 | **2D menu screens break in VR** (title / pause / results use ortho / HUD draws) | High | Medium | `VRMenu` detects menu state via game-state enum → routes the 2D frame into a head-locked `XrCompositionLayerQuad`. |
| R3 | **Telemetry unit calibration** — no ground truth in meters | High | Medium | Ship initial `SpeedScale` derived from MK64's community-reverse-engineered top speed (~64 km/h at 150cc). Provide live ImGui readout + sliders so user tunes until SimHub shows sane values. Document procedure in `Telemetry.md`. |
| R4 | **Fast3D matrix interception for per-eye projection** | Medium | High | Hook at the high-level N64 gu-math functions (`guPerspectiveF`/`guLookAtF`), not inside libultraship's low-level matrix multiply. Low surface area, called once per camera setup. Fallback: override at `Engine` level before `run_one_game_iter`. |
| R5 | **DirectInput FFB vs SDL rumble coexistence** — double-open or starvation | Medium | Medium | For devices matched as wheels (by GUID), disable libultraship's SDL rumble path; route rumble through `FFBManager`. Gamepad SDL rumble unchanged. |
| R6 | **PSVR2 on PC quirks** — SteamVR OpenXR layer can intermittently fail Sense controller bindings | Medium | Medium | Use OpenXR interaction profile `/interaction_profiles/khr/simple_controller` as a universal fallback; try `/interaction_profiles/valve/index_controller` first (PSVR2 Sense maps through that on SteamVR). Log bound actions at startup. |
| R7 | **Frame-interpolation coverage** — if `FrameInterpolation` only interpolates camera, not actors, VR will shimmer | Medium | High | Audit `FrameInterpolation.cpp` in Phase 5; extend to actor matrices. Budget: up to 1 week. |
| R8 | **Telemetry worker thread sendto() blocks** | Low | Medium | Lockless SPSC ring; worker thread owned by `Telemetry` class; non-blocking socket. |
| R9 | **Licensing** — accidental inclusion of proprietary SDKs | Low | High | Avoid Logitech/Fanatec/Thrustmaster SDKs entirely. OpenXR Apache-2.0, SDL2 zlib, DirectInput is Win32 system API, DirectX is system. Document in `LICENSES.md`. |

## Outstanding unknowns — please supply if you can

| # | Unknown | Why it matters | Ideal artifact |
|---|---|---|---|
| 1 | **Exact name of the MK64 game-state enum** that distinguishes title / menu / race / pause / results | Needed for `VRMenu` to switch between projection and quad layers | Grep result or user pointer — I'll find it in Phase 0 but if you already know the symbol name it saves a pass |
| 2 | **Project CARS 2 UDP v2 packet definitions** (`SMS_UDP_Definitions.hpp`) — Slightly Mad forum post | Correct byte-for-byte packet layout | PDF or GitHub gist URL; there are several community mirrors but the canonical v2 header file is best |
| 3 | **Empirical MK64 top speed in real-world km/h for calibration seed** | First-cut `SpeedScale` value | Any speedrun community post or wiki ("100cc = ~50 km/h, 150cc = ~64 km/h" style) |
| 4 | **Confirmation you have a legal MK64 ROM** | Phase 0 build requires it; I won't touch it in planning | Just a yes — the ROM stays local, never committed |
| 5 | **PSVR2 PC Adapter + SteamVR driver version you're running** | SteamVR OpenXR runtime quirks differ by version | `about` panel screenshot or version string |
| 6 | **Do you have the Sense controllers, or will you use a separate wheel-only input scheme?** | VRInput action bindings + whether we implement wrist-mounted menu | Yes/no |
| 7 | **SimHub version + is the "Custom Serial Devices" or "Motion" plugin installed?** | We target that plugin's expected telemetry interface | Version + plugin list |
| 8 | **Confirm current `ControlDeck` subclass in VRmicelliKart** (game implements `WriteToPad`) | Exact injection point for wheel axis routing | File path — I'll find it in Phase 0 |

## Verification plan

End-to-end test after each phase:

- **Phase 1 (telemetry):** Launch game in practice mode on Mario Raceway → launch SimHub → confirm live telemetry dash shows speed, RPM, G-force changing with driving. Run `Wireshark` on loopback 5606 to confirm 60 Hz packet rate and correct PC2 header.
- **Phase 2a (wheel input):** Plug in T300 → open Wheel menu → rebind hop to wheel paddle → drive a lap using only the wheel; verify steering feels linear.
- **Phase 2b (FFB):** Drive on grass → feel rumble; hit wall → feel collision pulse; release wheel on straight → feel centering return to zero.
- **Phase 3 (VR camera, no stereo):** Toggle VR mode in desktop → camera switches to first-person anchored on kart; mouse look works.
- **Phase 4 (stereo):** Put on PSVR2 → confirm stereo depth (close one eye, note parallax on nearby objects); recenter works; Alt-Tab cleanly survives XR session loss.
- **Phase 5 (comfort):** 20-minute race session with snap-turn + vignette; survey for nausea. Menus are readable and selectable in HMD.
- **Phase 6 (integration):** All three features on simultaneously. Measure frame time (should stay ≤11.1 ms for 90 Hz). Run SimHub + motion rig if available; confirm packet rate is not affected by VR load.

Automated tests added per phase:
- `tests/telemetry_packet_test.cpp` — golden-file PC2/DR2 packet assembly
- `tests/wheel_curve_test.cpp` — curve/deadzone/DOR math
- `tests/vr_pose_math_test.cpp` — head-pose → camera conversion

## Commit / PR strategy

- Every phase → one PR per repo (VRmicelliKart + libultraship when both touched)
- Commit messages: `[PhaseN] <scope>: <what>` (e.g., `[Phase1] telemetry: add PC2 packet encoder`)
- Every commit updates `CHANGELOG.md`
- `docs/*.md` and README files stay in sync commit-by-commit, never batched