#ifndef SKY_H
#define SKY_H

#ifdef __cplusplus

#include <libultraship/libultraship.h>
#include <libultraship/libultra/gbi.h>

#include "SkyActor.h"

#include "defines.h"

EXTERN_C_START
#include "code_800029B0.h"
EXTERN_C_END

class Sky {
public:
    static Sky* Instance;
    Sky();
    virtual void Draw(ScreenContext* screen);
    virtual void DrawFloor(ScreenContext* screen);
    void DrawVRSky(ScreenContext* screen);
    Sky* GetSky();
    void SetColours(Vtx* skybox);
    void InitActors(ScreenContext* screen);
    std::vector<std::unique_ptr<SkyActor>>& GetSkyActors();
private:
    Mtx mSkyboxMatrix;
    static Vtx mSkyboxScreenOne[8];
    static Vtx mSkyboxScreenTwo[8];
    static Vtx mSkyboxScreenThree[8];
    static Vtx mSkyboxScreenFour[8];

    static Vtx mVRSkyVtx[17];
    static Vtx mVRFloorVtx[17];

    std::vector<std::unique_ptr<SkyActor>> mSkyActors;
};
#endif // __cplusplus

/** C Compatible functions **/
EXTERN_C void InitSkyActors(ScreenContext* screen);
EXTERN_C void TickSkyActors();
EXTERN_C void DrawSkyActors(ScreenContext* screen, s32 arg0);
EXTERN_C void CM_RaceDrawVRSky(ScreenContext* screen);

#endif // SKY_H
