# VRmicelliKart Phasing Plan

## Phasing Strategy

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
