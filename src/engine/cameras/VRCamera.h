#pragma once

#include "GameCamera.h"

class VRCamera : public GameCamera {
public:
    VRCamera(FVector pos, s16 rot, u32 mode);

    virtual void Tick() override;
    virtual void SetViewProjection() override;
};
