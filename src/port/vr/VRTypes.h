#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float projectionMatrix[4][4];
    float viewMatrix[4][4];
} VREyeData;

#ifdef __cplusplus
}
#endif
