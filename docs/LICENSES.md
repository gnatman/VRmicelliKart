# Licenses

This project integrates several libraries and APIs with differing licenses.

## Core Dependencies

| Component | License | Role |
|---|---|---|
| **OpenXR Loader** | Apache 2.0 | VR API infrastructure |
| **SDL2** | zlib | Window management, input (except FFB) |
| **DirectInput8** | MS System API | Force-Feedback (Windows) |
| **Direct3D 11** | MS System API | VR Rendering backend |
| **libultraship** | [See libultraship/LICENSE] | RCP emulation and runtime |
| **SpaghettiKart** | [See VRmicelliKart/README.md] | MK64 game layer base |

## Project Code
The modifications for VRmicelliKart and libultraship VR-Runtime are provided under the same license as their respective base projects.

## Proprietary SDKs
This project explicitly avoids proprietary SDKs (e.g., Logitech, Fanatec, Thrustmaster) to ensure maximum compatibility and avoid licensing complications.
