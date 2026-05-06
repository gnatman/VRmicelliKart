#pragma once

#include <libultraship.h>
#include "ship/window/gui/Gui.h"
#include "ship/window/Window.h"

class Gui; // <-- forward declare
//class Window;

#include <fast/Fast3dGui.h>

namespace Ship {
    class SpaghettiGui : public Fast::Fast3dGui {
      public:
        SpaghettiGui() : Fast::Fast3dGui() {}
        SpaghettiGui(std::vector<std::shared_ptr<GuiWindow>> guiWindows) : Fast::Fast3dGui(guiWindows) {}

      protected:
        virtual void DrawMenu() override;
    };
}