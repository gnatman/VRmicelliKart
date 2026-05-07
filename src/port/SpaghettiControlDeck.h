#pragma once
#include "libultraship/controller/controldeck/ControlDeck.h"

class SpaghettiControlDeck : public LUS::ControlDeck {
public:
    using LUS::ControlDeck::ControlDeck;
    void WriteToPad(void* pad) override;
};
