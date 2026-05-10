#include <libultraship.h>
#include <libultra/gbi.h>
#include "GameCamera.h"
#include "port/interpolation/FrameInterpolation.h"
#include "engine/Matrix.h"
#include "port/Game.h"
#include <ship/Context.h>
#include <math.h>

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

// Helper to convert LookAt parameters to a quaternion representing the camera's world orientation
static void LookAtToQuaternion(float* eye, float* at, float* up, float* q) {
    float f[3] = { at[0] - eye[0], at[1] - eye[1], at[2] - eye[2] };
    float len = sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (len > 0.0001f) {
        f[0] /= len; f[1] /= len; f[2] /= len;
    } else {
        f[0] = 0.0f; f[1] = 0.0f; f[2] = -1.0f;
    }

    float s[3] = { f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0] };
    len = sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    if (len > 0.0001f) {
        s[0] /= len; s[1] /= len; s[2] /= len;
    } else {
        s[0] = 1.0f; s[1] = 0.0f; s[2] = 0.0f;
    }

    float u[3] = { s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0] };

    // The camera's world matrix columns are [right, up, -forward]
    float m00 = s[0], m01 = u[0], m02 = -f[0];
    float m10 = s[1], m11 = u[1], m12 = -f[1];
    float m20 = s[2], m21 = u[2], m22 = -f[2];

    float tr = m00 + m11 + m22;
    if (tr > 0) {
        float S = sqrt(tr + 1.0f) * 2;
        q[3] = 0.25f * S;
        q[0] = (m21 - m12) / S;
        q[1] = (m02 - m20) / S;
        q[2] = (m10 - m01) / S;
    } else if ((m00 > m11) && (m00 > m22)) {
        float S = sqrt(1.0f + m00 - m11 - m22) * 2;
        q[3] = (m21 - m12) / S;
        q[0] = 0.25f * S;
        q[1] = (m01 + m10) / S;
        q[2] = (m02 + m20) / S;
    } else if (m11 > m22) {
        float S = sqrt(1.0f + m11 - m00 - m22) * 2;
        q[3] = (m02 - m20) / S;
        q[0] = (m01 + m10) / S;
        q[1] = 0.25f * S;
        q[2] = (m12 + m21) / S;
    } else {
        float S = sqrt(1.0f + m22 - m00 - m11) * 2;
        q[3] = (m10 - m01) / S;
        q[0] = (m02 + m20) / S;
        q[1] = (m12 + m21) / S;
        q[2] = 0.25f * S;
    }
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

    // Update VR base tracking space
    auto window = Ship::Context::GetInstance()->GetWindow();
    if (window != nullptr) {
        float q[4];
        Player* player = &gPlayers[_camera->playerId];
        int cameraMode = CVarGetInteger("gVRCameraMode", 0);
        
        float basePos[3];
        if (cameraMode == 1) { // Cockpit
            // Put the camera at the player position + vertical offset for head height.
            // We use the player's orientation matrix to keep the head height relative to the kart's tilt.
            // orientationMatrix[i][1] is the UP vector.
            float headHeight = 5.0f;
            basePos[0] = player->pos[0] + player->orientationMatrix[0][1] * headHeight;
            basePos[1] = player->pos[1] + player->orientationMatrix[1][1] * headHeight;
            basePos[2] = player->pos[2] + player->orientationMatrix[2][1] * headHeight;
        } else {
            basePos[0] = _camera->pos[0];
            basePos[1] = _camera->pos[1];
            basePos[2] = _camera->pos[2];
        }

        // The camera target (lookAt) needs to be instantly locked to the kart's orientation.
        // Invert the X components to fix inverted steering rotation.
        float kart_at[3] = {
            basePos[0] - player->orientationMatrix[0][2], // Inverted X
            basePos[1] + player->orientationMatrix[1][2],
            basePos[2] + player->orientationMatrix[2][2]
        };
        float kart_up[3] = {
            -player->orientationMatrix[0][1], // Inverted X
            player->orientationMatrix[1][1],
            player->orientationMatrix[2][1]
        };
        
        LookAtToQuaternion(basePos, kart_at, kart_up, q);
        window->SetVRBaseTrackingSpace(basePos, q);
    }

    // Calculate the camera lookAt (camera rotation)
    guLookAt(&LookAtMatrix, _camera->pos[0], _camera->pos[1], _camera->pos[2], _camera->lookAt[0],
             _camera->lookAt[1], _camera->lookAt[2], _camera->up[0], _camera->up[1], _camera->up[2]);
    gSPMatrix(gDisplayListHead++, &LookAtMatrix,
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);

    FrameInterpolation_RecordCloseChild();
}
