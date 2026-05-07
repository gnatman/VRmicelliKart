# VR Mode (OpenXR)

## Overview
VRmicelliKart provides an immersive stereoscopic first-person experience utilizing OpenXR and Direct3D 11. It is optimized for PC VR, specifically targeted and tested with the PSVR2 via the Sony PC Adapter.

## Setup Requirements
- **OS:** Windows only (requires D3D11 backend).
- **Runtime:** SteamVR OpenXR runtime (or compatible).
- **HMD:** PSVR2 (recommended), Oculus, WMR, or Valve Index.

## CVars (Configuration)
Accessible via the ImGui "VR" menu or console:

| CVar | Default | Purpose |
|---|---|---|
| `gVR.Enabled` | 0 | Master switch (requires D3D11 backend + restart) |
| `gVR.IPDScale` | 1.0 | Scales eye-separation relative to world |
| `gVR.WorldScale` | 1.0 | Scales kart/world relative to player (cockpit feel) |
| `gVR.CameraMode` | 0 | 0=cockpit, 1=chase, 2=hood |
| `gVR.HUDDistance` | 1.5 | World-locked HUD depth (meters) |
| `gVR.MenuLockMode` | 1 | 0=world, 1=head, 2=wrist |
| `gVR.SnapTurn` | 0 | Comfort option (0/1) |
| `gVR.Vignette` | 0.5 | Lateral-G comfort vignette (0-1) |

## Implementation Details
- **Stereoscopic Rendering:** Uses Fast3D's `CreateFramebuffer` and `StartDrawToFramebuffer` to render per-eye into OpenXR swapchain images.
- **Interpolation:** Game logic ticks at 30 Hz, but VR renders at 90/120 Hz. `FrameInterpolation` is extended to upsample actor matrices and camera poses.
- **HUD & Menus:** 2D interfaces (Title, Pause, Results) are rendered into a flat framebuffer and submitted as an `XrCompositionLayerQuad`, placed in front of the user's face.
- **Comfort:** Implements lateral-G vignettes and snap-turning to mitigate motion sickness during high-speed drifting.
