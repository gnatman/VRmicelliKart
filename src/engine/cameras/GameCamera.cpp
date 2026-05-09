#include <libultraship.h>
#include <libultra/gbi.h>
#include "GameCamera.h"
#include "port/interpolation/FrameInterpolation.h"
#include "engine/Matrix.h"
#include "port/Game.h"
#include <ship/Context.h>

extern "C" {
#include "main.h"
#include "code_800029B0.h"
#include "camera.h"
}

size_t GameCamera::_count = 0;

GameCamera::GameCamera() {
    _camera = &cameras[_count];
    _camera->cameraId = _count;
    _camera->perspectiveMatrix = &PerspectiveMatrix;
    _camera->lookAtMatrix = &LookAtMatrix;
    spdlog::warn("GameCamera::GameCamera() instantiated (index {})", _count);

    _count += 1;
}

GameCamera::GameCamera(FVector pos, s16 rot, u32 mode) {
    _camera = &cameras[_count];
    _camera->cameraId = _count;
    _camera->perspectiveMatrix = &PerspectiveMatrix;
    _camera->lookAtMatrix = &LookAtMatrix;
    spdlog::warn("GameCamera(FVector, s16, u32) instantiated (index {})", _count);
    switch(gGamestate) {
        case RACING:
            _camera->renderMode = RENDER_TRACK_SECTIONS;
            break;
        case ENDING:
        case CREDITS_SEQUENCE:
            _camera->renderMode = RENDER_FULL_SCENE;
            break;
    }
    ProjMode = ProjectionMode::PERSPECTIVE;
    bActive = true;
    if (gGamestate != CREDITS_SEQUENCE) {
        Vec3f spawn = {pos.x, pos.y, pos.z};
        camera_init(spawn, rot, mode, _camera->cameraId);
    }

    _count += 1;
}

void GameCamera::Tick() {
    if (!bActive) { return; }
    if (nullptr == _camera) {
        bActive = false;
        return;
    }

    // Log the camera position to debug tracking
    spdlog::info("Camera Pos: ({}, {}, {})", _camera->pos[0], _camera->pos[1], _camera->pos[2]);

    func_8001EE98(&gPlayers[_camera->playerId], _camera, _camera->cameraId);
}

void GameCamera::SetActive(bool state) {
    bActive = state;
}

bool GameCamera::IsActive() {
    return bActive;
}

Camera* GameCamera::Get() {
    return _camera;
}

void GameCamera::SetProjectionMode(GameCamera::ProjectionMode mode) {
    ProjMode = mode;
}

Mtx* GameCamera::GetPerspMatrix() {
    return &PerspectiveMatrix;
}

Mtx* GameCamera::GetLookAtMatrix() {
    return &LookAtMatrix;
}

void GameCamera::SetViewProjection() {
    u16 perspNorm;

    // Tag the camera for the interpolation engine
    FrameInterpolation_RecordOpenChild("camera",
                                       (FrameInterpolation_GetCameraEpoch() | ((_camera->cameraId << 8))));

    // Calculate camera perspective (camera movement/location)
    guPerspective(&PerspectiveMatrix, &perspNorm, _camera->fieldOfView, gScreenAspect,
                  CM_GetProps()->NearPersp, CM_GetProps()->FarPersp, 1.0f);
    gSPPerspNormalize(gDisplayListHead++, perspNorm);
    gSPMatrix(gDisplayListHead++, &PerspectiveMatrix,
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

#ifdef ENABLE_VR
    auto window = Context::GetInstance()->GetWindow();
    if (window != nullptr) {
        auto headPose = window->GetVRHeadPose();
        if (headPose != nullptr) {
            float x = headPose->orientation[0];
            float y = headPose->orientation[1];
            float z = headPose->orientation[2];
            float w = headPose->orientation[3];

            float xx = x * x;
            float xy = x * y;
            float xz = x * z;
            float xw = x * w;
            float yy = y * y;
            float yz = y * z;
            float yw = y * w;
            float zz = z * z;
            float zw = z * w;

            float m00 = 1.0f - 2.0f * (yy + zz);
            float m01 = 2.0f * (xy - zw);
            float m02 = 2.0f * (xz + yw);
            float m10 = 2.0f * (xy + zw);
            float m11 = 1.0f - 2.0f * (xx + zz);
            float m12 = 2.0f * (yz - xw);
            float m20 = 2.0f * (xz - yw);
            float m21 = 2.0f * (yz + xw);
            float m22 = 1.0f - 2.0f * (xx + yy);

            float dx = lookAt.x - pos.x;
            float dy = lookAt.y - pos.y;
            float dz = lookAt.z - pos.z;

            lookAt.x = pos.x + (m00 * dx + m01 * dy + m02 * dz);
            lookAt.y = pos.y + (m10 * dx + m11 * dy + m12 * dz);
            lookAt.z = pos.z + (m20 * dx + m21 * dy + m22 * dz);
        }
    }
#endif

    // Calculate the camera lookAt (camera rotation)
    guLookAt(&LookAtMatrix, _camera->pos[0], _camera->pos[1], _camera->pos[2], _camera->lookAt[0],
             _camera->lookAt[1], _camera->lookAt[2], _camera->up[0], _camera->up[1], _camera->up[2]);
    gSPMatrix(gDisplayListHead++, &LookAtMatrix,
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);

    FrameInterpolation_RecordCloseChild();
}
