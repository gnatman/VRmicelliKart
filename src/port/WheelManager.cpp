#include "WheelManager.h"
#include <imgui.h>
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/utils/StringHelper.h"
#include <spdlog/spdlog.h>

WheelManager* WheelManager::mInstance = nullptr;

WheelManager* WheelManager::GetInstance() {
    if (mInstance == nullptr) {
        mInstance = new WheelManager();
    }
    return mInstance;
}

WheelManager::WheelManager() {
    LoadSettings();
}

WheelManager::~WheelManager() {
    CloseJoystick();
}

void WheelManager::Init() {
    RefreshJoysticks();
}

void WheelManager::LoadSettings() {
    mSteeringAxis = CVarGetInteger("gWheel.SteeringAxis", -1);
    mThrottleAxis = CVarGetInteger("gWheel.ThrottleAxis", -1);
    mBrakeAxis = CVarGetInteger("gWheel.BrakeAxis", -1);
    mDriftAxis = CVarGetInteger("gWheel.DriftAxis", -1);
    
    mSteeringInvert = CVarGetInteger("gWheel.SteeringInvert", 0);
    mThrottleInvert = CVarGetInteger("gWheel.ThrottleInvert", 0);
    mBrakeInvert = CVarGetInteger("gWheel.BrakeInvert", 0);
    mDriftInvert = CVarGetInteger("gWheel.DriftInvert", 0);
    
    mSteeringSensitivity = CVarGetFloat("gWheel.SteeringSensitivity", 1.0f);
    mSteeringDeadzone = CVarGetFloat("gWheel.SteeringDeadzone", 0.0f);
    mSteeringLinearity = CVarGetFloat("gWheel.SteeringLinearity", 1.0f);
    mSteeringCenter = CVarGetInteger("gWheel.SteeringCenter", 0);
    mThrottleThreshold = CVarGetFloat("gWheel.ThrottleThreshold", 0.5f);
    mBrakeThreshold = CVarGetFloat("gWheel.BrakeThreshold", 0.5f);
    mDriftThreshold = CVarGetFloat("gWheel.DriftThreshold", 0.5f);

    mJoystickGuid = CVarGetString("gWheel.JoystickGuid", "");

    std::vector<uint16_t> buttons = { 
        BTN_A, BTN_B, BTN_START, BTN_L, BTN_R, BTN_Z, 
        BTN_CUP, BTN_CDOWN, BTN_CLEFT, BTN_CRIGHT,
        BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT 
    };
    for (auto btn : buttons) {
        // We now use "Btn" prefix to avoid JSON numeric key crashes
        std::string cvarPrefix = StringHelper::Sprintf("gWheel.Button.Btn%04X", btn);
        int mappingCount = CVarGetInteger((cvarPrefix + ".Count").c_str(), 0);
        
        mButtonMap[btn].clear();

        if (mappingCount > 0) {
            for (int i = 0; i < mappingCount; i++) {
                std::string subPrefix = StringHelper::Sprintf("%s.Bind%d", cvarPrefix.c_str(), i);
                MappingType type = (MappingType)CVarGetInteger((subPrefix + ".Type").c_str(), 0);
                int index = CVarGetInteger((subPrefix + ".Index").c_str(), -1);
                uint8_t value = (uint8_t)CVarGetInteger((subPrefix + ".Value").c_str(), 0);
                if (type != MappingType::None) {
                    mButtonMap[btn].push_back({ type, index, value });
                }
            }
        } else {
            // Check legacy / single-mapping CVars (including non-prefixed ones)
            std::string legacyPrefix = StringHelper::Sprintf("gWheel.Button.%04X", btn);
            int type = CVarGetInteger((cvarPrefix + ".Type").c_str(), 
                       CVarGetInteger((legacyPrefix + ".Type").c_str(), 0));
            
            if (type == 1) { // Button
                int idx = CVarGetInteger((cvarPrefix + ".Index").c_str(), 
                          CVarGetInteger((legacyPrefix + ".Index").c_str(), -1));
                mButtonMap[btn].push_back({ MappingType::Button, idx, 0 });
            } else if (type == 2) { // Hat
                int idx = CVarGetInteger((cvarPrefix + ".Index").c_str(), 
                          CVarGetInteger((legacyPrefix + ".Index").c_str(), -1));
                uint8_t val = (uint8_t)CVarGetInteger((cvarPrefix + ".Value").c_str(), 
                                       CVarGetInteger((legacyPrefix + ".Value").c_str(), 0));
                mButtonMap[btn].push_back({ MappingType::Hat, idx, val });
            } else {
                int legacyIdx = CVarGetInteger(cvarPrefix.c_str(), CVarGetInteger(legacyPrefix.c_str(), -1));
                if (legacyIdx != -1) {
                    mButtonMap[btn].push_back({ MappingType::Button, legacyIdx, 0 });
                }
            }
        }
    }
}

void WheelManager::SaveSettings() {
    CVarSetInteger("gWheel.SteeringAxis", mSteeringAxis);
    CVarSetInteger("gWheel.ThrottleAxis", mThrottleAxis);
    CVarSetInteger("gWheel.BrakeAxis", mBrakeAxis);
    CVarSetInteger("gWheel.DriftAxis", mDriftAxis);
    
    CVarSetInteger("gWheel.SteeringInvert", mSteeringInvert);
    CVarSetInteger("gWheel.ThrottleInvert", mThrottleInvert);
    CVarSetInteger("gWheel.BrakeInvert", mBrakeInvert);
    CVarSetInteger("gWheel.DriftInvert", mDriftInvert);

    CVarSetFloat("gWheel.SteeringSensitivity", mSteeringSensitivity);
    CVarSetFloat("gWheel.SteeringDeadzone", mSteeringDeadzone);
    CVarSetFloat("gWheel.SteeringLinearity", mSteeringLinearity);
    CVarSetInteger("gWheel.SteeringCenter", mSteeringCenter);

    CVarSetString("gWheel.JoystickGuid", mJoystickGuid.c_str());

    // Safely clear legacy/numeric mapping CVars to prevent JSON unflattening crashes.
    // We cannot use ClearBlock() here as it triggers a full config reload, reverting unsaved CVars.
    std::vector<uint16_t> allBtns = { 
        BTN_A, BTN_B, BTN_START, BTN_L, BTN_R, BTN_Z, 
        BTN_CUP, BTN_CDOWN, BTN_CLEFT, BTN_CRIGHT,
        BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT 
    };
    for (auto btn : allBtns) {
        std::string legacyPrefix = StringHelper::Sprintf("gWheel.Button.%04X", btn);
        CVarClear(legacyPrefix.c_str());
        CVarClear((legacyPrefix + ".Type").c_str());
        CVarClear((legacyPrefix + ".Index").c_str());
        CVarClear((legacyPrefix + ".Value").c_str());
        CVarClear((legacyPrefix + ".Count").c_str());
        for (int i = 0; i < 10; i++) { // Clear up to 10 potential numeric sub-mappings
            std::string subPrefix = StringHelper::Sprintf("%s.%d", legacyPrefix.c_str(), i);
            CVarClear((subPrefix + ".Type").c_str());
            CVarClear((subPrefix + ".Index").c_str());
            CVarClear((subPrefix + ".Value").c_str());
        }
    }

    for (auto const& [btn, mappings] : mButtonMap) {
        std::string cvarPrefix = StringHelper::Sprintf("gWheel.Button.Btn%04X", btn);
        
        // Clean up any previously saved prefixed sub-keys if we are shrinking the array
        int oldCount = CVarGetInteger((cvarPrefix + ".Count").c_str(), 0);
        for (int i = mappings.size(); i < oldCount; i++) {
            std::string subPrefix = StringHelper::Sprintf("%s.Bind%d", cvarPrefix.c_str(), i);
            CVarClear((subPrefix + ".Type").c_str());
            CVarClear((subPrefix + ".Index").c_str());
            CVarClear((subPrefix + ".Value").c_str());
        }

        if (mappings.empty()) {
            CVarClear((cvarPrefix + ".Count").c_str());
            continue;
        }

        CVarSetInteger((cvarPrefix + ".Count").c_str(), (int)mappings.size());
        
        for (int i = 0; i < mappings.size(); i++) {
            std::string subPrefix = StringHelper::Sprintf("%s.Bind%d", cvarPrefix.c_str(), i);
            CVarSetInteger((subPrefix + ".Type").c_str(), (int)mappings[i].type);
            CVarSetInteger((subPrefix + ".Index").c_str(), mappings[i].index);
            CVarSetInteger((subPrefix + ".Value").c_str(), mappings[i].hatValue);
        }
    }

    Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

void WheelManager::OpenJoystick(int index) {
    CloseJoystick();
    mJoystick = SDL_JoystickOpen(index);
    if (mJoystick) {
        mJoystickIndex = index;
        char guidStr[33];
        SDL_JoystickGUID guid = SDL_JoystickGetGUID(mJoystick);
        SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
        mJoystickGuid = guidStr;
        SPDLOG_INFO("Opened Racing Wheel: {}", SDL_JoystickName(mJoystick));
    }
}

void WheelManager::CloseJoystick() {
    if (mJoystick) {
        SDL_JoystickClose(mJoystick);
        mJoystick = nullptr;
        mJoystickIndex = -1;
    }
}

void WheelManager::RefreshJoysticks() {
    int numJoysticks = SDL_NumJoysticks();
    if (mJoystickGuid != "") {
        for (int i = 0; i < numJoysticks; ++i) {
            char guidStr[33];
            SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
            SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
            if (mJoystickGuid == guidStr) {
                OpenJoystick(i);
                return;
            }
        }
    }
}

bool WheelManager::IsEnabled() {
    return CVarGetInteger("gWheel.Enabled", 0) && mJoystick != nullptr;
}

void WheelManager::Update() {
    if (!mJoystick) {
        // Try to reconnect if guid is set
        if (mJoystickGuid != "") {
            RefreshJoysticks();
        }
        return;
    }

    // Check for mapping input
    if (mIsMappingButton) {
        // 1. Check Buttons
        for (int i = 0; i < SDL_JoystickNumButtons(mJoystick); ++i) {
            if (SDL_JoystickGetButton(mJoystick, i)) {
                // Check if this mapping already exists for this button
                bool exists = false;
                for (const auto& m : mButtonMap[mMappingButtonBitmask]) {
                    if (m.type == MappingType::Button && m.index == i) {
                        exists = true;
                        break;
                    }
                }
                
                if (!exists) {
                    mButtonMap[mMappingButtonBitmask].push_back({ MappingType::Button, i, 0 });
                    SaveSettings();
                }
                mIsMappingButton = false;
                return;
            }
        }

        // 2. Check Hats (POV)
        for (int i = 0; i < SDL_JoystickNumHats(mJoystick); ++i) {
            uint8_t hatValue = SDL_JoystickGetHat(mJoystick, i);
            if (hatValue != SDL_HAT_CENTERED) {
                // Check if this mapping already exists
                bool exists = false;
                for (const auto& m : mButtonMap[mMappingButtonBitmask]) {
                    if (m.type == MappingType::Hat && m.index == i && m.hatValue == hatValue) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    mButtonMap[mMappingButtonBitmask].push_back({ MappingType::Hat, i, hatValue });
                    SaveSettings();
                }
                mIsMappingButton = false;
                return;
            }
        }
    }

    if (mIsMappingAxis) {
        for (int i = 0; i < SDL_JoystickNumAxes(mJoystick); ++i) {
            int16_t value = SDL_JoystickGetAxis(mJoystick, i);
            if (abs(value) > 20000) { // Significant movement
                if (mMappingAxisType == 0) mSteeringAxis = i;
                else if (mMappingAxisType == 1) mThrottleAxis = i;
                else if (mMappingAxisType == 2) mBrakeAxis = i;
                
                mIsMappingAxis = false;
                SaveSettings();
                break;
            }
        }
    }
}

void WheelManager::ProcessInput(OSContPad* pad) {
    if (!IsEnabled()) return;

    // Steering
    if (mSteeringAxis != -1) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mSteeringAxis);
        
        // Apply center offset
        int32_t centered = (int32_t)raw - (int32_t)mSteeringCenter;
        float normalized = (float)centered / 32768.0f;
        
        if (mSteeringInvert) normalized = -normalized;
        
        // Deadzone
        if (abs(normalized) < mSteeringDeadzone) {
            normalized = 0.0f;
        } else {
            normalized = (normalized > 0 ? 1.0f : -1.0f) * (abs(normalized) - mSteeringDeadzone) / (1.0f - mSteeringDeadzone);
        }
        
        // Linearity (Gamma Curve)
        if (normalized != 0.0f) {
            normalized = (normalized > 0 ? 1.0f : -1.0f) * pow(abs(normalized), mSteeringLinearity);
        }
        
        // Sensitivity
        normalized *= mSteeringSensitivity;
        
        // Clamp to N64 range
        if (normalized > 1.0f) normalized = 1.0f;
        if (normalized < -1.0f) normalized = -1.0f;
        
        int8_t wheel_stick_x = (int8_t)(normalized * 127.0f);

        if (CVarGetInteger("gWheel.CombineInputs", 1)) {
            // Combine with existing input (e.g. keyboard)
            // We use the one with the greater absolute deflection
            if (abs(wheel_stick_x) > abs(pad->stick_x)) {
                pad->stick_x = wheel_stick_x;
            }
        } else {
            // Traditional behavior: Wheel overrides everything
            pad->stick_x = wheel_stick_x;
        }
    }

    // Throttle (A button)
    if (mThrottleAxis != -1) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mThrottleAxis);
        // Pedals often go from -32768 (unpressed) to 32767 (pressed) or 0 to 32767
        // We'll normalize based on a threshold
        bool pressed = mThrottleInvert ? (raw < 0) : (raw > 0);
        if (pressed) pad->button |= BTN_A;
    }

    // Brake (B button)
    if (mBrakeAxis != -1) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mBrakeAxis);
        bool pressed = mBrakeInvert ? (raw < 0) : (raw > 0);
        if (pressed) pad->button |= BTN_B;
    }
    
    // Drift (R button)
    if (mDriftAxis != -1) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mDriftAxis);
        bool pressed = mDriftInvert ? (raw < 0) : (raw > 0);
        if (pressed) pad->button |= BTN_R;
    }

    // Buttons & Hats
    for (auto const& [btn, mappings] : mButtonMap) {
        for (const auto& mapping : mappings) {
            if (mapping.type == MappingType::Button && mapping.index != -1) {
                if (SDL_JoystickGetButton(mJoystick, mapping.index)) {
                    pad->button |= btn;
                    break; // Action triggered, no need to check other mappings for THIS action
                }
            } else if (mapping.type == MappingType::Hat && mapping.index != -1) {
                uint8_t state = SDL_JoystickGetHat(mJoystick, mapping.index);
                if (state & mapping.hatValue) {
                    pad->button |= btn;
                    break;
                }
            }
        }
    }
}

void WheelManager::DrawSettings() {
    bool enabled = CVarGetInteger("gWheel.Enabled", 0);
    if (ImGui::Checkbox("Enable Racing Wheel", &enabled)) {
        CVarSetInteger("gWheel.Enabled", enabled);
        SaveSettings();
    }

    if (enabled && !mJoystick) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), ICON_FA_EXCLAMATION_TRIANGLE " No Wheel Detected");
    }

    ImGui::Separator();

    if (ImGui::Button("Refresh Joysticks")) {
        RefreshJoysticks();
    }

    ImGui::Text("Connected Joysticks:");
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; ++i) {
        bool isCurrent = (mJoystickIndex == i);
        if (ImGui::Selectable(StringHelper::Sprintf("%d: %s", i, SDL_JoystickNameForIndex(i)).c_str(), isCurrent)) {
            OpenJoystick(i);
            SaveSettings();
        }
    }

    if (mJoystick) {
        ImGui::Separator();
        ImGui::Text("Currently Using: %s", SDL_JoystickName(mJoystick));
        
        // Axes
        ImGui::Text("Axes Configuration:");
        
        auto DrawAxisMapping = [&](const char* label, int& axis, bool& invert, int type) {
            ImGui::PushID(label);
            ImGui::Text("%s:", label);
            ImGui::SameLine();
            
            int numAxes = SDL_JoystickNumAxes(mJoystick);
            std::string preview = (axis == -1) ? "None" : StringHelper::Sprintf("Axis %d", axis);
            
            if (ImGui::BeginCombo("##combo", preview.c_str())) {
                if (ImGui::Selectable("None", axis == -1)) {
                    axis = -1;
                    SaveSettings();
                }
                for (int i = 0; i < numAxes; ++i) {
                    bool selected = (axis == i);
                    if (ImGui::Selectable(StringHelper::Sprintf("Axis %d", i).c_str(), selected)) {
                        axis = i;
                        SaveSettings();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Checkbox("Invert", &invert)) {
                SaveSettings();
            }
            ImGui::PopID();
        };

        DrawAxisMapping("Steering", mSteeringAxis, mSteeringInvert, 0);
        
        if (mSteeringAxis != -1) {
            ImGui::Indent();
            if (ImGui::Button("Calibrate Center")) {
                mSteeringCenter = SDL_JoystickGetAxis(mJoystick, mSteeringAxis);
                SaveSettings();
            }
            ImGui::SameLine();
            ImGui::Text("Offset: %d", mSteeringCenter);

            if (ImGui::SliderFloat("Sensitivity", &mSteeringSensitivity, 0.1f, 3.0f, "%.2f")) SaveSettings();
            if (ImGui::SliderFloat("Deadzone", &mSteeringDeadzone, 0.0f, 0.5f, "%.2f")) SaveSettings();
            if (ImGui::SliderFloat("Linearity", &mSteeringLinearity, 0.5f, 3.0f, "%.2f")) SaveSettings();
            
            // Visualizer
            int16_t raw = SDL_JoystickGetAxis(mJoystick, mSteeringAxis);
            float val = (float)((int32_t)raw - (int32_t)mSteeringCenter) / 32768.0f;
            if (mSteeringInvert) val = -val;
            ImGui::ProgressBar((val + 1.0f) / 2.0f, ImVec2(-1, 0), StringHelper::Sprintf("Raw: %d", raw).c_str());
            ImGui::Unindent();
        }

        DrawAxisMapping("Throttle", mThrottleAxis, mThrottleInvert, 1);
        DrawAxisMapping("Brake", mBrakeAxis, mBrakeInvert, 2);
        DrawAxisMapping("Drift (R)", mDriftAxis, mDriftInvert, 3);

        ImGui::Separator();
        ImGui::Text("Button Configuration:");
        
        auto DrawButtonMapping = [&](const char* label, uint16_t bitmask) {
            ImGui::PushID(label);
            ImGui::Text("%s:", label);
            
            auto& mappings = mButtonMap[bitmask];
            
            for (int i = 0; i < mappings.size(); i++) {
                auto& mapping = mappings[i];
                std::string btnText = "Unknown";
                if (mapping.type == MappingType::Button) {
                    btnText = StringHelper::Sprintf("Button %d", mapping.index);
                } else if (mapping.type == MappingType::Hat) {
                    std::string dir = "Unknown";
                    if (mapping.hatValue & SDL_HAT_UP) dir = "Up";
                    else if (mapping.hatValue & SDL_HAT_DOWN) dir = "Down";
                    else if (mapping.hatValue & SDL_HAT_LEFT) dir = "Left";
                    else if (mapping.hatValue & SDL_HAT_RIGHT) dir = "Right";
                    btnText = StringHelper::Sprintf("POV %d %s", mapping.index, dir.c_str());
                }

                ImGui::SameLine();
                ImGui::PushID(i);
                if (ImGui::Button(StringHelper::Sprintf("%s ##clear", btnText.c_str()).c_str())) {
                    mappings.erase(mappings.begin() + i);
                    SaveSettings();
                    ImGui::PopID();
                    break; 
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Click to remove this mapping.");
                }
                ImGui::PopID();
            }

            ImGui::SameLine();
            if (ImGui::Button(StringHelper::Sprintf("+##add%04X", bitmask).c_str())) {
                mIsMappingButton = true;
                mMappingButtonBitmask = bitmask;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Add a new wheel mapping for this action.");
            }

            ImGui::PopID();
        };

        DrawButtonMapping("A (Accelerate)", BTN_A);
        DrawButtonMapping("B (Brake/Reverse)", BTN_B);
        DrawButtonMapping("Z (Item)", BTN_Z);
        DrawButtonMapping("R (Drift/Jump)", BTN_R);
        DrawButtonMapping("Start", BTN_START);
        DrawButtonMapping("L (Toggle Map)", BTN_L);
        
        if (ImGui::TreeNode("C-Buttons & D-Pad")) {
            DrawButtonMapping("C-Up", BTN_CUP);
            DrawButtonMapping("C-Down", BTN_CDOWN);
            DrawButtonMapping("C-Left", BTN_CLEFT);
            DrawButtonMapping("C-Right", BTN_CRIGHT);
            DrawButtonMapping("D-Up", BTN_DUP);
            DrawButtonMapping("D-Down", BTN_DDOWN);
            DrawButtonMapping("D-Left", BTN_DLEFT);
            DrawButtonMapping("D-Right", BTN_DRIGHT);
            ImGui::TreePop();
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No joystick selected or connected.");
    }

    if (mIsMappingButton || mIsMappingAxis) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Mapping...", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Press a button or move an axis on your wheel...");
            if (ImGui::Button("Cancel")) {
                mIsMappingButton = false;
                mIsMappingAxis = false;
            }
            ImGui::End();
        }
    }
}

void WheelManager::DrawUI() {
    if (ImGui::Begin("Racing Wheel Configuration")) {
        DrawSettings();
        if (ImGui::Button("Close")) {
            CVarSetInteger("gWheel.ConfigWindowOpen", 0);
            SaveSettings();
        }
    }
    ImGui::End();
}

extern "C" {
void WheelManager_Init() {
    WheelManager::GetInstance()->Init();
}

void WheelManager_Update() {
    WheelManager::GetInstance()->Update();
}

void WheelManager_ProcessInput(void* pad) {
    WheelManager::GetInstance()->ProcessInput((OSContPad*)pad);
}

void WheelManager_DrawUI() {
    if (CVarGetInteger("gWheel.ConfigWindowOpen", 0)) {
        WheelManager::GetInstance()->DrawUI();
    }
}

void WheelManager_DrawSettings() {
    WheelManager::GetInstance()->DrawSettings();
}
}
