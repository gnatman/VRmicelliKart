#include "FirstPersonCockpit.h"
#include <libultraship.h>
#include <libultra/gbi.h>
#include <cmath>

#include "port/WheelManager.h"
#include "port/interpolation/FrameInterpolation.h"
#include "engine/Matrix.h"
#include "port/Game.h"

extern "C" {
#include "racing/math_util.h"
#include "main.h"
#include "defines.h"
#include "common_structs.h"
#include "actor_types.h"
}

static bool sCockpitInitialized = false;

// Geometry buffers - 16 segment models for high roundness and fidelity
// 1. Steering wheel rim: 16 outer verts, 16 inner verts
static Vtx sWheelRimFrontVtx[32];     // 0..15 outer (Z=0), 16..31 inner (Z=0)
static Vtx sWheelRimSideOuterVtx[32]; // 0..15 front outer (Z=0), 16..31 back outer (Z=18)
static Vtx sWheelRimSideInnerVtx[32]; // 0..15 front inner (Z=0), 16..31 back inner (Z=18)

// 2. Center Hub: White disc, Red badge, White Mario "M" logo
static Vtx sWheelHubWhiteVtx[17];     // 0 is center, 1..16 perimeter at Z=-2
static Vtx sWheelHubChromeVtx[32];    // Outer trim ring
static Vtx sWheelBadgeRedVtx[17];     // 0 is center, 1..16 perimeter at Z=-3
static Vtx sWheelMLogoVtx[12];        // 12 vertices for crisp Mario "M" at Z=-4
static Vtx sWheelSpokesVtx[12];       // 3 straight spokes at Z=1
static Vtx sSteeringColumnVtx[12];    // Column tube going forward from Z=2 to Z=65

// 3. Tires: 16-segment cylinder with dual yellow rims and rolling tread
static Vtx sTireOuterRimVtx[17];      // Outer face yellow rim
static Vtx sTireInnerRimVtx[17];      // Inner face yellow rim
static Vtx sTireTreadVtx[32];         // 16 outer edge + 16 inner edge

// 4. Dashboard cowl
static Vtx sDashVtx[16];

static float sTireRollAngle = 0.0f;

static inline void SetVtx(Vtx& v, short x, short y, short z, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    v.v.ob[0] = x;
    v.v.ob[1] = y;
    v.v.ob[2] = z;
    v.v.flag = 0;
    v.v.tc[0] = 0;
    v.v.tc[1] = 0;
    v.v.cn[0] = static_cast<signed char>(r);
    v.v.cn[1] = static_cast<signed char>(g);
    v.v.cn[2] = static_cast<signed char>(b);
    v.v.cn[3] = static_cast<signed char>(a);
}

void FirstPersonCockpit_Init(void) {
    if (sCockpitInitialized) return;

    constexpr float kPi = 3.141592653589793f;
    constexpr int kRimSegments = 16;

    // =========================================================================
    // 1. Steering Wheel Rim (16 segments, smooth circular torus)
    // Outer radius: 100, Inner radius: 76.
    // Z=0 is front face facing driver. +Z goes away into dashboard.
    // =========================================================================
    for (int i = 0; i < kRimSegments; i++) {
        float angle = (float)i * (2.0f * kPi / (float)kRimSegments);
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        short ox = static_cast<short>(cosA * 100.0f);
        short oy = static_cast<short>(sinA * 100.0f);
        short ix = static_cast<short>(cosA * 76.0f);
        short iy = static_cast<short>(sinA * 76.0f);

        // Front Face: Outer (0..15) and Inner (16..31)
        uint8_t cOuter = static_cast<uint8_t>(34 + sinA * 6.0f);
        uint8_t cInner = static_cast<uint8_t>(22 + sinA * 4.0f);
        SetVtx(sWheelRimFrontVtx[i], ox, oy, 0, cOuter, cOuter, cOuter + 2);
        SetVtx(sWheelRimFrontVtx[16 + i], ix, iy, 0, cInner, cInner, cInner + 2);

        // Outer Bevel (front Z=0 to back Z=18)
        SetVtx(sWheelRimSideOuterVtx[i], ox, oy, 0, 28, 28, 30);
        SetVtx(sWheelRimSideOuterVtx[16 + i], ox, oy, 18, 16, 16, 18);

        // Inner Bevel (front Z=0 to back Z=18)
        SetVtx(sWheelRimSideInnerVtx[i], ix, iy, 0, 20, 20, 22);
        SetVtx(sWheelRimSideInnerVtx[16 + i], ix, iy, 18, 14, 14, 16);
    }

    // =========================================================================
    // 2. Center Hub: White circle, Red badge, White Mario "M" logo
    // All facing driver (-Z direction) so they are in front of the wheel rim.
    // =========================================================================
    // White disc: Center (0) + 16 perimeter points at radius 30, Z=-2
    SetVtx(sWheelHubWhiteVtx[0], 0, 0, -2, 245, 245, 250);
    for (int i = 0; i < 16; i++) {
        float angle = (float)i * (2.0f * kPi / 16.0f);
        short hx = static_cast<short>(std::cos(angle) * 30.0f);
        short hy = static_cast<short>(std::sin(angle) * 30.0f);
        SetVtx(sWheelHubWhiteVtx[1 + i], hx, hy, -2, 235, 235, 240);
    }

    // Red badge: Center (0) + 16 perimeter points at radius 22, Z=-3 (in front of white disc)
    SetVtx(sWheelBadgeRedVtx[0], 0, 0, -3, 230, 25, 25);
    for (int i = 0; i < 16; i++) {
        float angle = (float)i * (2.0f * kPi / 16.0f);
        short bx = static_cast<short>(std::cos(angle) * 22.0f);
        short by = static_cast<short>(std::sin(angle) * 22.0f);
        SetVtx(sWheelBadgeRedVtx[1 + i], bx, by, -3, 205, 18, 18);
    }

    // Mario "M" Logo (12 vertices at Z=-4, pure white)
    // Left upright bar: (-10, 8), (-13, 8), (-13, -7), (-10, -7)
    SetVtx(sWheelMLogoVtx[0], -10,  8, -4, 255, 255, 255);
    SetVtx(sWheelMLogoVtx[1], -14,  8, -4, 255, 255, 255);
    SetVtx(sWheelMLogoVtx[2], -14, -7, -4, 255, 255, 255);
    SetVtx(sWheelMLogoVtx[3], -10, -7, -4, 255, 255, 255);

    // Right upright bar: (14, 8), (10, 8), (10, -7), (14, -7)
    SetVtx(sWheelMLogoVtx[4],  14,  8, -4, 255, 255, 255);
    SetVtx(sWheelMLogoVtx[5],  10,  8, -4, 255, 255, 255);
    SetVtx(sWheelMLogoVtx[6],  10, -7, -4, 255, 255, 255);
    SetVtx(sWheelMLogoVtx[7],  14, -7, -4, 255, 255, 255);

    // Center V dip: (-10, 8), (0, -2), (10, 8), (0, -6)
    SetVtx(sWheelMLogoVtx[8],  -10,  8, -4, 255, 255, 255); // Top left inner
    SetVtx(sWheelMLogoVtx[9],   10,  8, -4, 255, 255, 255); // Top right inner
    SetVtx(sWheelMLogoVtx[10],   0, -2, -4, 255, 255, 255); // Center dip top
    SetVtx(sWheelMLogoVtx[11],   0, -7, -4, 255, 255, 255); // Center dip bottom

    // =========================================================================
    // 3. Three Straight Spokes (Left at 9 o'clock, Right at 3 o'clock, Bottom at 6 o'clock)
    // Matte gunmetal (65, 68, 75) at Z=1 (just behind the rim face)
    // =========================================================================
    // Left Spoke (9 o'clock): 4 verts
    SetVtx(sWheelSpokesVtx[0], -30,  9, 1, 75, 78, 85);
    SetVtx(sWheelSpokesVtx[1], -76,  9, 1, 70, 72, 80);
    SetVtx(sWheelSpokesVtx[2], -76, -9, 1, 60, 62, 70);
    SetVtx(sWheelSpokesVtx[3], -30, -9, 1, 65, 68, 75);

    // Right Spoke (3 o'clock): 4 verts
    SetVtx(sWheelSpokesVtx[4],  30,  9, 1, 75, 78, 85);
    SetVtx(sWheelSpokesVtx[5],  76,  9, 1, 70, 72, 80);
    SetVtx(sWheelSpokesVtx[6],  76, -9, 1, 60, 62, 70);
    SetVtx(sWheelSpokesVtx[7],  30, -9, 1, 65, 68, 75);

    // Bottom Spoke (6 o'clock): 4 verts
    SetVtx(sWheelSpokesVtx[8],   -9, -30, 1, 70, 72, 80);
    SetVtx(sWheelSpokesVtx[9],    9, -30, 1, 70, 72, 80);
    SetVtx(sWheelSpokesVtx[10],   9, -76, 1, 60, 62, 70);
    SetVtx(sWheelSpokesVtx[11],  -9, -76, 1, 60, 62, 70);

    // Steering Column tube: cylinder going FORWARD from Z=2 to Z=65 (into dashboard)
    for (int i = 0; i < 4; i++) {
        float angle = (float)i * (2.0f * kPi / 4.0f) + (kPi / 4.0f);
        short cx = static_cast<short>(std::cos(angle) * 14.0f);
        short cy = static_cast<short>(std::sin(angle) * 14.0f);
        SetVtx(sSteeringColumnVtx[i],     cx, cy,  2, 40, 42, 46);
        SetVtx(sSteeringColumnVtx[4 + i], cx, cy, 65, 22, 24, 26);
    }

    // =========================================================================
    // 4. Front Tires (16-segment cylinder with dual yellow rims)
    // Radius: 100, Width: 90 (from -45 to +45)
    // Both outer and inner faces have yellow rims so wheels look great from all angles.
    // =========================================================================
    constexpr int kTireSegments = 16;

    // Outer Face Rim (at X=+45)
    SetVtx(sTireOuterRimVtx[0], 45, 0, 0, 215, 220, 225); // Chrome center nut
    for (int i = 0; i < kTireSegments; i++) {
        float angle = (float)i * (2.0f * kPi / (float)kTireSegments);
        short ty = static_cast<short>(std::cos(angle) * 100.0f);
        short tz = static_cast<short>(std::sin(angle) * 100.0f);

        if (i % 2 == 0) {
            SetVtx(sTireOuterRimVtx[1 + i], 45, ty, tz, 245, 205, 20); // Bright Yellow spoke
        } else {
            SetVtx(sTireOuterRimVtx[1 + i], 42, ty, tz, 45, 48, 52);   // Dark recessed rim gap
        }
    }

    // Inner Face Rim (at X=-45)
    SetVtx(sTireInnerRimVtx[0], -45, 0, 0, 215, 220, 225); // Chrome center nut
    for (int i = 0; i < kTireSegments; i++) {
        float angle = (float)i * (2.0f * kPi / (float)kTireSegments);
        short ty = static_cast<short>(std::cos(angle) * 100.0f);
        short tz = static_cast<short>(std::sin(angle) * 100.0f);

        if (i % 2 == 0) {
            SetVtx(sTireInnerRimVtx[1 + i], -45, ty, tz, 245, 205, 20); // Bright Yellow spoke
        } else {
            SetVtx(sTireInnerRimVtx[1 + i], -42, ty, tz, 45, 48, 52);   // Dark recessed rim gap
        }
    }

    // Tread surface (16 outer edge verts 0..15 at X=+45, 16 inner edge verts 16..31 at X=-45)
    for (int i = 0; i < kTireSegments; i++) {
        float angle = (float)i * (2.0f * kPi / (float)kTireSegments);
        short ty = static_cast<short>(std::cos(angle) * 100.0f);
        short tz = static_cast<short>(std::sin(angle) * 100.0f);

        uint8_t cTread = (i % 2 == 0) ? 28 : 42;
        SetVtx(sTireTreadVtx[i],       45, ty, tz, cTread, cTread, cTread + 2);
        SetVtx(sTireTreadVtx[16 + i], -45, ty, tz, cTread - 4, cTread - 4, cTread - 2);
    }

    // =========================================================================
    // 5. Dashboard / Instrument Cowl (MK7 style curved dash hood)
    // =========================================================================
    // Front face (0..3)
    SetVtx(sDashVtx[0], -65,  38, 0, 48, 50, 55);
    SetVtx(sDashVtx[1],  65,  38, 0, 48, 50, 55);
    SetVtx(sDashVtx[2],  75, -35, 0, 38, 40, 44);
    SetVtx(sDashVtx[3], -75, -35, 0, 38, 40, 44);
    // Top hood curve (4..5)
    SetVtx(sDashVtx[4], -55,  52, 55, 36, 38, 42);
    SetVtx(sDashVtx[5],  55,  52, 55, 36, 38, 42);
    // Chassis base attachment (6..9)
    SetVtx(sDashVtx[6], -85, -55, 110, 25, 26, 28);
    SetVtx(sDashVtx[7],  85, -55, 110, 25, 26, 28);
    SetVtx(sDashVtx[8], -70,  20, 110, 30, 32, 35);
    SetVtx(sDashVtx[9],  70,  20, 110, 30, 32, 35);

    sCockpitInitialized = true;
}

static void BuildLocalMatrix(Mat4 mtx,
                             float posX, float posY, float posZ,
                             float rotPitchRad, float rotYawRad, float rotRollRad,
                             float scaleX, float scaleY, float scaleZ) {
    float cp = std::cos(rotPitchRad), sp = std::sin(rotPitchRad);
    float cy = std::cos(rotYawRad),   sy = std::sin(rotYawRad);
    float cr = std::cos(rotRollRad),  sr = std::sin(rotRollRad);

    mtx[0][0] = (cy * cr + sy * sp * sr) * scaleX;
    mtx[0][1] = (cp * sr) * scaleX;
    mtx[0][2] = (-sy * cr + cy * sp * sr) * scaleX;
    mtx[0][3] = 0.0f;

    mtx[1][0] = (-cy * sr + sy * sp * cr) * scaleY;
    mtx[1][1] = (cp * cr) * scaleY;
    mtx[1][2] = (sy * sr + cy * sp * cr) * scaleY;
    mtx[1][3] = 0.0f;

    mtx[2][0] = (sy * cp) * scaleZ;
    mtx[2][1] = (-sp) * scaleZ;
    mtx[2][2] = (cy * cp) * scaleZ;
    mtx[2][3] = 0.0f;

    mtx[3][0] = posX;
    mtx[3][1] = posY;
    mtx[3][2] = posZ;
    mtx[3][3] = 1.0f;
}

static void DrawSteeringWheelGeometry() {
    // 1. Front Rim Face (16 quads / 32 triangles)
    gSPVertex(gDisplayListHead++, (uintptr_t)sWheelRimFrontVtx, 32, 0);
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        gSP2Triangles(gDisplayListHead++,
            i, next, 16 + next, 0,
            i, 16 + next, 16 + i, 0);
    }

    // 2. Outer Rim Bevel (16 quads)
    gSPVertex(gDisplayListHead++, (uintptr_t)sWheelRimSideOuterVtx, 32, 0);
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        gSP2Triangles(gDisplayListHead++,
            i, next, 16 + next, 0,
            i, 16 + next, 16 + i, 0);
    }

    // 3. Inner Rim Bevel (16 quads)
    gSPVertex(gDisplayListHead++, (uintptr_t)sWheelRimSideInnerVtx, 32, 0);
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        gSP2Triangles(gDisplayListHead++,
            i, 16 + next, next, 0,
            i, 16 + i, 16 + next, 0);
    }

    // 4. Center Hub: White Circle Background
    gSPVertex(gDisplayListHead++, (uintptr_t)sWheelHubWhiteVtx, 17, 0);
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        gSP1Triangle(gDisplayListHead++, 0, 1 + i, 1 + next, 0);
    }

    // 5. Red Emblem Disc (in front of white circle)
    gSPVertex(gDisplayListHead++, (uintptr_t)sWheelBadgeRedVtx, 17, 0);
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        gSP1Triangle(gDisplayListHead++, 0, 1 + i, 1 + next, 0);
    }

    // 6. Crisp White Mario "M" Logo (in front of red emblem)
    gSPVertex(gDisplayListHead++, (uintptr_t)sWheelMLogoVtx, 12, 0);
    // Left upright pillar (0..3)
    gSP2Triangles(gDisplayListHead++,
        0, 1, 2, 0,
        0, 2, 3, 0);
    // Right upright pillar (4..7)
    gSP2Triangles(gDisplayListHead++,
        4, 5, 6, 0,
        4, 6, 7, 0);
    // Center V dip (8..11)
    gSP2Triangles(gDisplayListHead++,
        8, 10, 11, 0,
        8, 11, 9, 0);

    // 7. Three Straight Spokes (at Z=1, behind hub and rim face)
    gSPVertex(gDisplayListHead++, (uintptr_t)sWheelSpokesVtx, 12, 0);
    // Left Spoke (0..3)
    gSP2Triangles(gDisplayListHead++,
        0, 1, 2, 0,
        0, 2, 3, 0);
    // Right Spoke (4..7)
    gSP2Triangles(gDisplayListHead++,
        4, 5, 6, 0,
        4, 6, 7, 0);
    // Bottom Spoke (8..11)
    gSP2Triangles(gDisplayListHead++,
        8, 9, 10, 0,
        8, 10, 11, 0);

    // 8. Steering Column tube going FORWARD into the dashboard
    gSPVertex(gDisplayListHead++, (uintptr_t)sSteeringColumnVtx, 8, 0);
    for (int i = 0; i < 4; i++) {
        int next = (i + 1) % 4;
        gSP2Triangles(gDisplayListHead++,
            i, next, 4 + next, 0,
            i, 4 + next, 4 + i, 0);
    }
}

static void DrawTireGeometry() {
    // 1. Outer Face (Yellow rim with spokes and lug nut)
    gSPVertex(gDisplayListHead++, (uintptr_t)sTireOuterRimVtx, 17, 0);
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        gSP1Triangle(gDisplayListHead++, 0, 1 + i, 1 + next, 0);
    }

    // 2. Tread Surface (16 quads)
    gSPVertex(gDisplayListHead++, (uintptr_t)sTireTreadVtx, 32, 0);
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        gSP2Triangles(gDisplayListHead++,
            i, next, 16 + next, 0,
            i, 16 + next, 16 + i, 0);
    }

    // 3. Inner Face (Yellow rim with spokes and lug nut)
    gSPVertex(gDisplayListHead++, (uintptr_t)sTireInnerRimVtx, 17, 0);
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        gSP1Triangle(gDisplayListHead++, 0, 1 + next, 1 + i, 0);
    }
}

static void GetRainbowColor(float phase, uint8_t& r, uint8_t& g, uint8_t& b) {
    float h = std::fmod(phase, 1.0f) * 6.0f;
    int sector = static_cast<int>(h);
    float frac = h - static_cast<float>(sector);
    float q = 1.0f - frac;
    switch (sector) {
        case 0: r = 255; g = static_cast<uint8_t>(frac * 255.0f); b = 0; break;
        case 1: r = static_cast<uint8_t>(q * 255.0f); g = 255; b = 0; break;
        case 2: r = 0; g = 255; b = static_cast<uint8_t>(frac * 255.0f); break;
        case 3: r = 0; g = static_cast<uint8_t>(q * 255.0f); b = 255; break;
        case 4: r = static_cast<uint8_t>(frac * 255.0f); g = 0; b = 255; break;
        case 5: default: r = 255; g = 0; b = static_cast<uint8_t>(q * 255.0f); break;
    }
}

static void DrawDashboardGeometry() {
    // 1. Dashboard Cowl (hood)
    gSPVertex(gDisplayListHead++, (uintptr_t)sDashVtx, 10, 0);
    // Cowl face
    gSP2Triangles(gDisplayListHead++,
        0, 1, 2, 0,
        0, 2, 3, 0);
    // Upper cowl slope
    gSP2Triangles(gDisplayListHead++,
        0, 4, 5, 0,
        0, 5, 1, 0);
    // Side slopes
    gSP2Triangles(gDisplayListHead++,
        3, 6, 4, 0,
        3, 4, 0, 0);
    gSP2Triangles(gDisplayListHead++,
        1, 5, 7, 0,
        1, 7, 2, 0);
    // Rear hood transition
    gSP2Triangles(gDisplayListHead++,
        4, 8, 9, 0,
        4, 9, 5, 0);
}


void FirstPersonCockpit_Render(Player* player, Camera* camera, s8 playerId, s8 screenId) {
    if (!sCockpitInitialized) {
        FirstPersonCockpit_Init();
    }

    // Only render for the local human player when in Cockpit VR mode
    int cameraMode = CVarGetInteger("gVRCameraMode", 0);
    if (cameraMode != 1) return;
    if (playerId != screenId) return;
    if (CVarGetInteger("gVRDrawCockpit", 1) != 1) return;

    // Calculate head base position matching GameCamera's VR base tracking space exactly
    float headHeight = CVarGetFloat("gVRHeadHeight", 5.0f);
    float basePos[3] = {
        player->pos[0] + player->orientationMatrix[0][1] * headHeight,
        player->pos[1] + player->orientationMatrix[1][1] * headHeight,
        player->pos[2] + player->orientationMatrix[2][1] * headHeight
    };

    // Build the Cockpit Base 4x4 matrix using the player's true world orientation (anchored at driver head)
    Mat4 mtxCockpitBase;
    mtxCockpitBase[0][0] =  player->orientationMatrix[0][0];
    mtxCockpitBase[0][1] =  player->orientationMatrix[1][0];
    mtxCockpitBase[0][2] =  player->orientationMatrix[2][0];
    mtxCockpitBase[0][3] =  0.0f;

    mtxCockpitBase[1][0] =  player->orientationMatrix[0][1];
    mtxCockpitBase[1][1] =  player->orientationMatrix[1][1];
    mtxCockpitBase[1][2] =  player->orientationMatrix[2][1];
    mtxCockpitBase[1][3] =  0.0f;

    mtxCockpitBase[2][0] =  player->orientationMatrix[0][2];
    mtxCockpitBase[2][1] =  player->orientationMatrix[1][2];
    mtxCockpitBase[2][2] =  player->orientationMatrix[2][2];
    mtxCockpitBase[2][3] =  0.0f;

    mtxCockpitBase[3][0] = basePos[0];
    mtxCockpitBase[3][1] = basePos[1];
    mtxCockpitBase[3][2] = basePos[2];
    mtxCockpitBase[3][3] = 1.0f;

    // Set 3D render state: smooth shading, Z-buffer, opaque surface
    gSPSetGeometryMode(gDisplayListHead++, G_SHADING_SMOOTH | G_ZBUFFER);
    gSPClearGeometryMode(gDisplayListHead++, G_LIGHTING | G_CULL_BOTH);
    gDPSetRenderMode(gDisplayListHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);

    // Star Power: Smooth cycling rainbow lighting across cockpit geometry
    bool isStarActive = ((player->effects & STAR_EFFECT) != 0);
    uint8_t starR = 255, starG = 255, starB = 255;
    if (isStarActive) {
        float phase = std::fmod((float)gCourseTimer * 0.08f, 1.0f);
        GetRainbowColor(phase, starR, starG, starB);
        gDPSetPrimColor(gDisplayListHead++, 0, 0, starR, starG, starB, 180);
        gDPSetCombineLERP(gDisplayListHead++, PRIMITIVE, SHADE, PRIMITIVE_ALPHA, SHADE, 0, 0, 0, 1,
                                              PRIMITIVE, SHADE, PRIMITIVE_ALPHA, SHADE, 0, 0, 0, 1);
    } else {
        gDPSetCombineMode(gDisplayListHead++, G_CC_SHADE, G_CC_SHADE);
    }

    // Get player steering input [-1.0f, +1.0f]
    float steerFactor = 0.0f;
    if (WheelManager_IsNativeSteerActive() && (player->type & PLAYER_HUMAN)) {
        steerFactor = WheelManager_GetNativeSteer();
    } else if (playerId >= 0 && playerId < 4) {
        steerFactor = (float)gControllers[playerId].rawStickX / 65.0f;
        if (steerFactor > 1.0f) steerFactor = 1.0f;
        if (steerFactor < -1.0f) steerFactor = -1.0f;
    }

    // Steering wheel rotation angle (clockwise when turning right)
    constexpr float kMaxWheelAngleRad = 1.7453f; // ~100 degrees
    float steerAngleWheel = steerFactor * kMaxWheelAngleRad;

    // Front tires steering yaw angle (~22 degrees max, pivots right when turning right)
    constexpr float kMaxTireTurnRad = 0.384f; // ~22 degrees
    float steerAngleTire = -steerFactor * kMaxTireTurnRad;

    // Update tire rolling angle based on forward speed
    sTireRollAngle += (player->speed * 0.12f);
    if (sTireRollAngle > 6.28318f) sTireRollAngle -= 6.28318f;
    if (sTireRollAngle < 0.0f) sTireRollAngle += 6.28318f;

    constexpr float kCockpitTiltRad = 0.44f; // ~25 degrees tilted back

    // -------------------------------------------------------------
    // Adjustable Cockpit Geometry Offsets (CVars)
    // -------------------------------------------------------------
    float tireX = CVarGetFloat("gVRTireX", 3.84f);
    float tireY = CVarGetFloat("gVRTireY", -4.13f);
    float tireZ = CVarGetFloat("gVRTireZ", 4.31f);
    float tireScale = CVarGetFloat("gVRTireScale", 0.020f);

    float wheelY = CVarGetFloat("gVRWheelY", -2.40f);
    float wheelZ = CVarGetFloat("gVRWheelZ", 1.48f);
    float wheelScale = CVarGetFloat("gVRWheelScale", 0.020f);

    float dashY = wheelY - 0.5f;
    float dashZ = wheelZ + 0.3f;

    // -------------------------------------------------------------
    // 1. Dashboard Cowl (Hood)
    // -------------------------------------------------------------
    Mat4 mtxLocalDash, mtxFinalDash;
    BuildLocalMatrix(mtxLocalDash, 0.0f, dashY, dashZ, kCockpitTiltRad, 0.0f, 0.0f, 0.020f, 0.020f, 0.020f);
    mtxf_multiplication(mtxFinalDash, mtxLocalDash, mtxCockpitBase);
    AddCockpitMatrix(mtxFinalDash, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    DrawDashboardGeometry();

    // -------------------------------------------------------------
    // 2. Steering Wheel (rotated by steerAngleWheel)
    // -------------------------------------------------------------
    Mat4 mtxLocalWheel, mtxFinalWheel;
    BuildLocalMatrix(mtxLocalWheel, 0.0f, wheelY, wheelZ, kCockpitTiltRad, 0.0f, steerAngleWheel, wheelScale, wheelScale, wheelScale);
    mtxf_multiplication(mtxFinalWheel, mtxLocalWheel, mtxCockpitBase);
    AddCockpitMatrix(mtxFinalWheel, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    DrawSteeringWheelGeometry();

    // -------------------------------------------------------------
    // 3. Front Left Tire
    // -------------------------------------------------------------
    Mat4 mtxLocalLeftTire, mtxFinalLeftTire;
    BuildLocalMatrix(mtxLocalLeftTire, -tireX, tireY, tireZ, sTireRollAngle, steerAngleTire, 0.0f, tireScale, tireScale, tireScale);
    mtxf_multiplication(mtxFinalLeftTire, mtxLocalLeftTire, mtxCockpitBase);
    AddCockpitMatrix(mtxFinalLeftTire, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    DrawTireGeometry();

    // -------------------------------------------------------------
    // 4. Front Right Tire
    // -------------------------------------------------------------
    Mat4 mtxLocalRightTire, mtxFinalRightTire;
    BuildLocalMatrix(mtxLocalRightTire, tireX, tireY, tireZ, sTireRollAngle, steerAngleTire, 0.0f, tireScale, tireScale, tireScale);
    mtxf_multiplication(mtxFinalRightTire, mtxLocalRightTire, mtxCockpitBase);
    AddCockpitMatrix(mtxFinalRightTire, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    DrawTireGeometry();
}

extern "C" s16 gPlayerHeldItem[4] = { 0, 0, 0, 0 };

extern "C" s16 GetPlayerHeldItem(s32 playerId) {
    if (playerId < 0 || playerId >= 4) return ITEM_NONE;

    // 1. Scan active actors in the game (flags != 0 means active)
    size_t actorCount = CM_GetActorSize();
    for (size_t i = 0; i < actorCount; i++) {
        struct Actor* actor = CM_GetActor(i);
        if (!actor || actor->flags == 0) continue;

        switch (actor->type) {
            case ACTOR_BANANA: {
                struct BananaActor* banana = reinterpret_cast<struct BananaActor*>(actor);
                if (banana->playerId == playerId && (banana->state == HELD_BANANA || banana->state == FIRST_BANANA_BUNCH_BANANA)) {
                    if (banana->parentIndex != -1) {
                        return ITEM_BANANA_BUNCH;
                    }
                    return ITEM_BANANA;
                }
                break;
            }
            case ACTOR_BANANA_BUNCH: {
                struct BananaBunchParent* bunch = reinterpret_cast<struct BananaBunchParent*>(actor);
                if (bunch->playerId == playerId && bunch->bananasAvailable > 0) {
                    return ITEM_BANANA_BUNCH;
                }
                break;
            }
            case ACTOR_GREEN_SHELL: {
                struct ShellActor* shell = reinterpret_cast<struct ShellActor*>(actor);
                if (shell->playerId == playerId && shell->state == HELD_SHELL) {
                    return ITEM_GREEN_SHELL;
                }
                break;
            }
            case ACTOR_RED_SHELL: {
                struct ShellActor* shell = reinterpret_cast<struct ShellActor*>(actor);
                if (shell->playerId == playerId && shell->state == HELD_SHELL) {
                    return ITEM_RED_SHELL;
                }
                break;
            }
            case ACTOR_BLUE_SPINY_SHELL: {
                struct ShellActor* shell = reinterpret_cast<struct ShellActor*>(actor);
                if (shell->playerId == playerId && shell->state == HELD_SHELL) {
                    return ITEM_BLUE_SPINY_SHELL;
                }
                break;
            }
            case ACTOR_FAKE_ITEM_BOX: {
                struct FakeItemBox* box = reinterpret_cast<struct FakeItemBox*>(actor);
                if ((s32)box->playerId == playerId && box->state == HELD_FAKE_ITEM_BOX) {
                    return ITEM_FAKE_ITEM_BOX;
                }
                break;
            }
            default:
                break;
        }
    }

    // 2. Fallback: check player triggers & gPlayerHeldItem
    Player* player = &gPlayers[playerId];
    if ((player->triggers & DRAG_ITEM_EFFECT) && gPlayerHeldItem[playerId] > ITEM_NONE && gPlayerHeldItem[playerId] < ITEM_MAX) {
        return gPlayerHeldItem[playerId];
    }

    gPlayerHeldItem[playerId] = ITEM_NONE;
    return ITEM_NONE;
}

