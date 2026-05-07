#include "SpaghettiControlDeck.h"
#include "enhancements/wheel/WheelTuning.h"
#include "libultraship/libultra/controller.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include "ship/config/ConsoleVariable.h"

void SpaghettiControlDeck::WriteToPad(void* pad) {
    LUS::ControlDeck::WriteToPad(pad);
    
    OSContPad* pads = (OSContPad*)pad;
    
    if (CVarGetInteger("gWheel.Enabled", 0)) {
        // Apply tuning to player 1's stick
        WheelTuning_ApplyCurve(&pads[0].stick_x, 0.0f); // Speed currently unused in curve
    }
}
