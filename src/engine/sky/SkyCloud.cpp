#include <libultraship.h>
#include <libultra/gbi.h>
#include "SkyCloud.h"
#include <cmath>
#include <vector>
#include "engine/tracks/Track.h"
#include "engine/World.h"

#include "port/Engine.h"
#include "port/Game.h"
#include "port/interpolation/FrameInterpolation.h"

extern "C" {
#include "update_objects.h"
#include "code_80057C60.h"
#include "code_8006E9C0.h"
#include "assets/models/common_data.h"
#include "math_util.h"
#include "math_util_2.h"
#include "render_objects.h"
}

size_t SkyCloud::_count = 0;

SkyCloud::SkyCloud(ScreenContext* screen, u16 cloudVariant, u16 posY, u16 rotY, u16 scalePercent) : SkyActor(screen) {
    _idx = _count;
    mScreen = screen;
    mCloudVariant = cloudVariant;
    mY = posY;
    mRotY = rotY;
    mScale = (f32) scalePercent / 100.0;
    mTextureWidth = 64;
    mTextureHeight = 32;

    // Stock
    if (GameEngine_ResourceGetTexTypeByName((const char*)CM_GetProps()->CloudTexture) != 1) {
        mTexture = ((u8*) LOAD_ASSET_RAW(CM_GetProps()->CloudTexture)) + (cloudVariant * 1024);
        mVtx = (Vtx*)D_0D005FB0;
    } else { // Texture pack
        mTexture = CM_GetProps()->CloudTexture;

        if ((strcmp((const char*)CM_GetProps()->CloudTexture, gTextureExhaust3) == 0) ||
           (strcmp((const char*)CM_GetProps()->CloudTexture, gTextureExhaust4) == 0) ||
           (strcmp((const char*)CM_GetProps()->CloudTexture, gTextureExhaust5) == 0)) {
            mVtx = cloudvtx[cloudVariant];
        } else if (strcmp((const char*)CM_GetProps()->CloudTexture, gTextureExhaust0) == 0 ||
            strcmp((const char*)CM_GetProps()->CloudTexture, gTextureExhaust1) == 0 ||
            strcmp((const char*)CM_GetProps()->CloudTexture, gTextureExhaust2) == 0) {
            mVtx = cloudvtx2[cloudVariant];
        } else {
            mVtx = cloudvtx2[cloudVariant];
        }
    }

    _count += 1;
}

void SkyCloud::Tick() { // func_800788F8
    s16 cameraRot;

    s16 mUnk200 = mScreen->camera->fieldOfView + 40.0f;
    mUnk208 = ((mUnk200 / 2) * 0xB6) + 0x71C;
    mUnk210 = (-(mUnk200 / 2) * 0xB6) - 0x71C;
    mUnk1E8 = 1.7578125 / mUnk200;
    mUnk218 = SCREEN_WIDTH / 2;

    // Adjustable culling factor
    const float cullingFactor = OTRGetAspectRatio();

    // Calculate the cloud's rotation relative to the camera
    cameraRot = (u16)mScreen->camera->rot[1] + (u16)mRotY;
    // Adjust bounds based on the culling factor
    s16 adjustedLowerBound = (s16) (mUnk210 * cullingFactor);
    s16 adjustedUpperBound = (s16) (mUnk208 * cullingFactor);

    // Check if the object is within the adjusted bounds
    if ((cameraRot >= adjustedLowerBound) && (adjustedUpperBound >= cameraRot)) {
        // Calculate and update the object's X position
        // 160 (SCREEN_WIDTH / 2) + (D_8018D1E8 * cameraRot);
        // Grab center of screen, scale by fov factor, offset based on camera rotation
        mX = mUnk218 + (mUnk1E8 * cameraRot);

        // Mark the object as visible
        mVisible = true;
    } else {
        // If outside the bounds, mark the object as not visible
        mVisible = false;
    }
}

void SkyCloud::Draw(ScreenContext* screen, s32 arg0) { // render_clouds
   // Object* object = &gObjectList[_objectIndex];
    s32 posY = arg0 - mY;
    func_8004B6C4(255, 255, 255);
    // Skip drawing the object this frame if it warped to the other side of the screen
    if ((fabs(mX - mOldX) > SCREEN_WIDTH / 2) || (fabs(posY - mOldY) > SCREEN_HEIGHT / 2)) {
        mOldX = mX;
        mOldY = posY;
        return;
    }
    if (mVisible) {
        FrameInterpolation_RecordOpenChild("render_clouds", TAG_CLOUDS((_idx << 4) | (mScreen - gScreenContexts)));

        if (D_8018D228 != mCloudVariant) {
            D_8018D228 = mCloudVariant;
            func_80044DA0(mTexture, mTextureWidth, mTextureHeight);
        }
        func_80042330_unchanged(mX, posY, 0, mScale);
        gSPVertex(gDisplayListHead++, (uintptr_t)mVtx, 4, 0);
        gSPDisplayList(gDisplayListHead++, (Gfx*)common_rectangle_display);

        FrameInterpolation_RecordCloseChild();
    }
    mOldX = mX;
    mOldY = posY;
}

void SkyCloud::DrawVR(ScreenContext* screen) {
    Camera* camera = screen->camera;
    if (camera == nullptr || mTexture == nullptr || mVtx == nullptr) {
        return;
    }

    // Direction angle around horizon in radians:
    // In MK64 heading angle is negated relative to rot[1]
    float yawAngle = -(float)mRotY * (2.0f * 3.14159265358979323846f / 65536.0f);

    // Elevation angle above horizon in radians (mY typically ranges from -10 to +80)
    float pitchDeg = 8.0f + (float)mY * 0.28f;
    float pitchRad = pitchDeg * (3.14159265358979323846f / 180.0f);

    float R_cloud = 7400.0f;
    float R_horiz = R_cloud * cosf(pitchRad);

    Vec3f relPos;
    relPos[0] = R_horiz * sinf(yawAngle);
    relPos[1] = R_cloud * sinf(pitchRad);
    relPos[2] = R_horiz * cosf(yawAngle);

    Vec3f worldPos;
    worldPos[0] = camera->pos[0] + relPos[0];
    worldPos[1] = camera->pos[1] + relPos[1];
    worldPos[2] = camera->pos[2] + relPos[2];

    // Orientation: billboard facing back towards camera
    Vec3s rot;
    rot[0] = (s16)(-pitchDeg * (65536.0f / 360.0f));
    rot[1] = (s16)(-(s32)mRotY + 32768);
    rot[2] = 0;

    Mat4 mtx;
    mtxf_pos_rotation_xyz(mtx, worldPos, rot);
    mtxf_scale(mtx, mScale * 38.0f);

    if (render_set_position(mtx, 0) != 0) {
        FrameInterpolation_RecordOpenChild("render_clouds_vr", TAG_CLOUDS((_idx << 4) | (screen - gScreenContexts)));
        if (D_8018D228 != mCloudVariant) {
            D_8018D228 = mCloudVariant;
            func_80044DA0(mTexture, mTextureWidth, mTextureHeight);
        }
        gSPVertex(gDisplayListHead++, (uintptr_t)mVtx, 4, 0);
        gSPDisplayList(gDisplayListHead++, (Gfx*)common_rectangle_display);
        FrameInterpolation_RecordCloseChild();
    }
}
