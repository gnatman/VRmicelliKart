#include "ImguiUI.h"
#include "UIWidgets.h"
#include "ResolutionEditor.h"
#include "FreecamWindow.h"
#include "Tools.h"
#include "SceneExplorer.h"
#include "Properties.h"
#include "TrackProperties.h"
#include "ContentBrowser.h"

#include <spdlog/spdlog.h>
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ship/Context.h"

#include <imgui_internal.h>
#include <libultraship/libultraship.h>
#include <fast/Fast3dWindow.h>
#include <fast/Fast3dGui.h>
#include "port/Engine.h"
#include "PortMenu.h"

extern "C" {
extern s32 gGamestateNext;
extern s32 gMenuSelection;
#include "audio/external.h"
#include "defines.h"
}

namespace GameUI {
// std::shared_ptr<GameMenuBar> mGameMenuBar;
std::shared_ptr<PortMenu> mPortMenu;
std::shared_ptr<Ship::GuiWindow> mConsoleWindow;
std::shared_ptr<Ship::GuiWindow> mStatsWindow;
std::shared_ptr<Ship::GuiWindow> mInputEditorWindow;
std::shared_ptr<Ship::GuiWindow> mGfxDebuggerWindow;
std::shared_ptr<Ship::GuiWindow> mToolsWindow;
std::shared_ptr<Ship::GuiWindow> mSceneExplorerWindow;
std::shared_ptr<Ship::GuiWindow> mPropertiesWindow;
std::shared_ptr<Ship::GuiWindow> mTrackPropertiesWindow;
std::shared_ptr<Ship::GuiWindow> mContentBrowserWindow;

void SetupGuiElements() {
    auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();

    // mGameMenuBar = std::make_shared<GameMenuBar>("gOpenMenuBar", CVarGetInteger("gOpenMenuBar", 0));
    // gui->SetMenuBar(mGameMenuBar);

    mPortMenu = std::make_shared<PortMenu>("gOpenMenu", "Port Menu");
    gui->SetMenu(mPortMenu);

    mStatsWindow = gui->GetGuiWindow("Stats");
    if (mStatsWindow == nullptr) {
        SPDLOG_ERROR("Could not find stats window");
    }

    mConsoleWindow = gui->GetGuiWindow("Console");
    if (mConsoleWindow == nullptr) {
        SPDLOG_ERROR("Could not find console window");
    }

    mInputEditorWindow = gui->GetGuiWindow("Input Editor");
    if (mInputEditorWindow == nullptr) {
        SPDLOG_ERROR("Could not find input editor window");
        return;
    }

    mGfxDebuggerWindow = gui->GetGuiWindow("GfxDebuggerWindow");
    if (mGfxDebuggerWindow == nullptr) {
        SPDLOG_ERROR("Could not find input GfxDebuggerWindow");
    }

    mToolsWindow = std::make_shared<TrackEditor::ToolsWindow>("gEditorEnabled", "Tools", ImVec2(100, 100),
                                                                  (ImGuiWindowFlags_NoTitleBar));
    gui->AddGuiWindow(mToolsWindow);

    mSceneExplorerWindow = std::make_shared<TrackEditor::SceneExplorerWindow>("gEditorEnabled", "Scene Explorer");
    gui->AddGuiWindow(mSceneExplorerWindow);

    mPropertiesWindow = std::make_shared<TrackEditor::PropertiesWindow>("gEditorEnabled", "Properties");
    gui->AddGuiWindow(mPropertiesWindow);

    mTrackPropertiesWindow = std::make_shared<TrackEditor::TrackPropertiesWindow>("gEditorEnabled", "Track Properties");
    gui->AddGuiWindow(mTrackPropertiesWindow);

    mContentBrowserWindow =
        std::make_shared<TrackEditor::ContentBrowserWindow>("gEditorEnabled", "Content Browser");
    gui->AddGuiWindow(mContentBrowserWindow);
}

void Destroy() {
    mConsoleWindow = nullptr;
    mStatsWindow = nullptr;
    mInputEditorWindow = nullptr;
    mToolsWindow = nullptr;
    mSceneExplorerWindow = nullptr;
    mPropertiesWindow = nullptr;
    mTrackPropertiesWindow = nullptr;
    mContentBrowserWindow = nullptr;
}

std::string GetWindowButtonText(const char* text, bool menuOpen) {
    char buttonText[100] = "";
    if (menuOpen) {
        strcat(buttonText, ICON_FA_CHEVRON_RIGHT " ");
    }
    strcat(buttonText, text);
    if (!menuOpen) {
        strcat(buttonText, "  ");
    }
    return buttonText;
}
} // namespace GameUI

static const char* filters[3] = {
#ifdef __WIIU__
    "",
#else
    "Three-Point",
#endif
    "Linear", "None"
};

void DrawSettingsMenu() {
}

void DrawMenuBarIcon() {
    static bool gameIconLoaded = false;
    if (!gameIconLoaded) {
        // Ship::Context::GetInstance()->GetWindow()->GetGui()->LoadTexture("Game_Icon",
        // "textures/icons/gIcon.png");
        gameIconLoaded = false;
    }

    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetInstance()->GetWindow()->GetGui());
    if (gui && gui->GetTextureByName("Game_Icon")) {
#ifdef __SWITCH__
        ImVec2 iconSize = ImVec2(20.0f, 20.0f);
        float posScale = 1.0f;
#elif defined(__WIIU__)
        ImVec2 iconSize = ImVec2(16.0f * 2, 16.0f * 2);
        float posScale = 2.0f;
#else
        ImVec2 iconSize = ImVec2(20.0f, 20.0f);
        float posScale = 1.5f;
#endif
        ImGui::SetCursorPos(ImVec2(5, 2.5f) * posScale);
        ImGui::Image(gui->GetTextureByName("Game_Icon"), iconSize);
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(25, 0) * posScale);
    }
}

void DrawGameMenu() {
}

void DrawEnhancementsMenu() {
    if (UIWidgets::BeginMenu("Enhancements")) {

        if (UIWidgets::BeginMenu("Gameplay")) {
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

void DrawCheatsMenu() {
}

const char* debugInfoPages[6] = {
    "Object", "Check Surface", "Map", "Stage", "Effect", "Enemy",
};

void DrawDebugMenu() {
}

void GameMenuBar::DrawElement() {
    if (ImGui::BeginMenuBar()) {
        DrawMenuBarIcon();

        DrawGameMenu();

        ImGui::SetCursorPosY(0.0f);

        // DrawSettingsMenu();

        ImGui::SetCursorPosY(0.0f);

        // DrawEnhancementsMenu();

        ImGui::SetCursorPosY(0.0f);

        // DrawCheatsMenu();

        ImGui::SetCursorPosY(0.0f);

        // DrawDebugMenu();

        ImGui::EndMenuBar();
    }
}
