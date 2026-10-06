#include "WheelManager.h"
#include "port/TelemetryManager.h"
#include <imgui.h>
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/utils/StringHelper.h"
#include <spdlog/spdlog.h>
#include <algorithm>

extern "C" {
#include "common_structs.h"
#include "defines.h"
#include "mk64.h"
extern Player gPlayers[];
extern s32 gGamestate;
extern s32 gRaceState;
extern u16 gDemoMode;
}

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
    mSteeringSaturation = CVarGetFloat("gWheel.SteeringSaturation", 1.0f);
    mSteeringSCurve = CVarGetFloat("gWheel.SteeringSCurve", 0.0f);
    mSteeringCenter = CVarGetInteger("gWheel.SteeringCenter", 0);
    mThrottleThreshold = CVarGetFloat("gWheel.ThrottleThreshold", 0.5f);
    mBrakeThreshold = CVarGetFloat("gWheel.BrakeThreshold", 0.5f);
    mDriftThreshold = CVarGetFloat("gWheel.DriftThreshold", 0.5f);
    mFFBEnabled = CVarGetInteger("gWheel.FFBEnabled", 1);
    mFFBMasterGain = CVarGetInteger("gWheel.FFBMasterGain", 100);
    mCombineInputs = CVarGetInteger("gWheel.CombineInputs", 1);

    mFFBEnableCentering = CVarGetInteger("gWheel.FFBEnableCentering", 1);
    mFFBEnableLateral = CVarGetInteger("gWheel.FFBEnableLateral", 1);
    mFFBEnableCollisionJolts = CVarGetInteger("gWheel.FFBEnableCollisionJolts", 1);
    mFFBEnableOffroadRumble = CVarGetInteger("gWheel.FFBEnableOffroadRumble", 1);
    mFFBEnableSpinoutShake = CVarGetInteger("gWheel.FFBEnableSpinoutShake", 1);

    mSoftLockEnabled = CVarGetInteger("gWheel.SoftLockEnabled", 1);
    mHardwareDOR = CVarGetInteger("gWheel.HardwareDOR", 900);
    mSoftLockAngle = CVarGetFloat("gWheel.SoftLockAngle", 90.0f);
    mSoftLockStiffness = CVarGetFloat("gWheel.SoftLockStiffness", 1.0f);
    mFFBInvert = CVarGetInteger("gWheel.FFBInvert", 1);

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
    CVarSetFloat("gWheel.SteeringSaturation", mSteeringSaturation);
    CVarSetFloat("gWheel.SteeringSCurve", mSteeringSCurve);
    CVarSetInteger("gWheel.SteeringCenter", mSteeringCenter);

    CVarSetInteger("gWheel.FFBEnabled", mFFBEnabled ? 1 : 0);
    CVarSetInteger("gWheel.FFBMasterGain", mFFBMasterGain);
    CVarSetInteger("gWheel.CombineInputs", mCombineInputs);
    CVarSetFloat("gWheel.ThrottleThreshold", mThrottleThreshold);
    CVarSetFloat("gWheel.BrakeThreshold", mBrakeThreshold);
    CVarSetFloat("gWheel.DriftThreshold", mDriftThreshold);

    CVarSetInteger("gWheel.FFBEnableCentering", mFFBEnableCentering ? 1 : 0);
    CVarSetInteger("gWheel.FFBEnableLateral", mFFBEnableLateral ? 1 : 0);
    CVarSetInteger("gWheel.FFBEnableCollisionJolts", mFFBEnableCollisionJolts ? 1 : 0);
    CVarSetInteger("gWheel.FFBEnableOffroadRumble", mFFBEnableOffroadRumble ? 1 : 0);
    CVarSetInteger("gWheel.FFBEnableSpinoutShake", mFFBEnableSpinoutShake ? 1 : 0);

    CVarSetInteger("gWheel.SoftLockEnabled", mSoftLockEnabled ? 1 : 0);
    CVarSetInteger("gWheel.HardwareDOR", mHardwareDOR);
    CVarSetFloat("gWheel.SoftLockAngle", mSoftLockAngle);
    CVarSetFloat("gWheel.SoftLockStiffness", mSoftLockStiffness);
    CVarSetInteger("gWheel.FFBInvert", mFFBInvert ? 1 : 0);

    CVarSetString("gWheel.JoystickGuid", mJoystickGuid.c_str());

    // Safely clear legacy/numeric mapping CVars to prevent JSON unflattening crashes.
    // We cannot use ClearBlock() here as it triggers a full config reload, reverting unsaved CVars.
    std::vector<uint16_t> allBtns = { 
        BTN_A, BTN_B, BTN_START, BTN_L, BTN_R, BTN_Z, 
        BTN_CUP, BTN_CDOWN, BTN_CLEFT, BTN_CRIGHT,
        BTN_DUP, BTN_DDOWN, BTN_DLEFT, BTN_DRIGHT 
    };
    for (auto btn : allBtns) {
        // Clear all possible legacy naming variants to prevent unflattening crashes
        std::vector<std::string> variants = {
            StringHelper::Sprintf("gWheel.Button.%04X", btn), // Padded hex (e.g. 0020)
            StringHelper::Sprintf("gWheel.Button.%X", btn),   // Unpadded hex (e.g. 20)
            StringHelper::Sprintf("gWheel.Button.%d", btn)    // Decimal (e.g. 32)
        };

        for (const auto& legacyPrefix : variants) {
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

        mHapticErrorStr = "";

        if (SDL_JoystickIsHaptic(mJoystick)) {
            mHaptic = SDL_HapticOpenFromJoystick(mJoystick);
            if (mHaptic) {
                unsigned int features = SDL_HapticQuery(mHaptic);
                SPDLOG_INFO("Haptic Features: 0x{:04X}", features);
                if (features & SDL_HAPTIC_CONSTANT) SPDLOG_INFO(" - Supports CONSTANT");
                if (features & SDL_HAPTIC_SINE) SPDLOG_INFO(" - Supports SINE");
                if (features & SDL_HAPTIC_TRIANGLE) SPDLOG_INFO(" - Supports TRIANGLE");
                if (features & SDL_HAPTIC_SPRING) SPDLOG_INFO(" - Supports SPRING");
                if (features & SDL_HAPTIC_DAMPER) SPDLOG_INFO(" - Supports DAMPER");
                if (features & SDL_HAPTIC_INERTIA) SPDLOG_INFO(" - Supports INERTIA");
                if (features & SDL_HAPTIC_FRICTION) SPDLOG_INFO(" - Supports FRICTION");
                if (features & SDL_HAPTIC_CUSTOM) SPDLOG_INFO(" - Supports CUSTOM");
                if (features & SDL_HAPTIC_GAIN) {
                    SPDLOG_INFO(" - Supports GAIN");
                    SDL_HapticSetGain(mHaptic, mFFBMasterGain);
                }
                if (features & SDL_HAPTIC_AUTOCENTER) {
                    SPDLOG_INFO(" - Supports AUTOCENTER");
                    if (SDL_HapticSetAutocenter(mHaptic, 0) == 0) {
                        SPDLOG_INFO(" - Disabled hardware AUTOCENTER");
                    } else {
                        SPDLOG_WARN(" - Failed to disable hardware AUTOCENTER: {}", SDL_GetError());
                    }
                }

                if (SDL_HapticRumbleSupported(mHaptic)) {
                    if (SDL_HapticRumbleInit(mHaptic) == 0) {
                        mHapticRumbleSupported = true;
                        SPDLOG_INFO(" - Rumble Initialized successfully");
                    } else {
                        SPDLOG_ERROR(" - RumbleInit failed: {}", SDL_GetError());
                    }
                } else {
                    SPDLOG_INFO(" - Rumble NOT supported");
                }

                InitHapticEffects();
                SPDLOG_INFO("Haptic Feedback Initialization Attempt Complete!");
            } else {
                mHapticErrorStr = SDL_GetError();
                SPDLOG_ERROR("SDL_HapticOpenFromJoystick failed: {}", mHapticErrorStr);
            }
        } else {
            mHapticErrorStr = SDL_GetError();
            SPDLOG_WARN("SDL_JoystickIsHaptic returned false for this device. Error: {}", mHapticErrorStr);
        }
    }
}

void WheelManager::InitHapticEffects() {
    if (!mHaptic) return;

    if (mConstantEffectId == -1) {
        SDL_HapticEffect effect;
        memset(&effect, 0, sizeof(SDL_HapticEffect));
        effect.type = SDL_HAPTIC_CONSTANT;
        effect.constant.direction.type = SDL_HAPTIC_STEERING_AXIS;
        effect.constant.direction.dir[0] = 1;
        effect.constant.level = 0;
        effect.constant.length = SDL_HAPTIC_INFINITY;
        mConstantEffectId = SDL_HapticNewEffect(mHaptic, &effect);
        mConstantDirectionType = SDL_HAPTIC_STEERING_AXIS;

        if (mConstantEffectId == -1) {
            SPDLOG_WARN("STEERING_AXIS constant effect failed, trying CARTESIAN: {}", SDL_GetError());
            effect.constant.direction.type = SDL_HAPTIC_CARTESIAN;
            mConstantEffectId = SDL_HapticNewEffect(mHaptic, &effect);
            mConstantDirectionType = SDL_HAPTIC_CARTESIAN;
        }

        if (mConstantEffectId != -1) {
            if (SDL_HapticRunEffect(mHaptic, mConstantEffectId, SDL_HAPTIC_INFINITY) != 0) {
                SPDLOG_ERROR("Failed to run CONSTANT effect: {}", SDL_GetError());
            } else {
                SPDLOG_INFO("CONSTANT effect created and running with type {} (INFINITY iterations).",
                    (mConstantDirectionType == SDL_HAPTIC_STEERING_AXIS) ? "STEERING_AXIS" : "CARTESIAN");
            }
        } else {
            SPDLOG_ERROR("Failed to create CONSTANT effect: {}", SDL_GetError());
        }
    }
}

void WheelManager::ReInitHapticEffects() {
    if (!mHaptic) return;
    SPDLOG_INFO("Reinitializing haptic effects (Re-arm)...");
    if (mConstantEffectId != -1) {
        SDL_HapticDestroyEffect(mHaptic, mConstantEffectId);
        mConstantEffectId = -1;
    }
    if (mSineEffectId != -1) {
        SDL_HapticDestroyEffect(mHaptic, mSineEffectId);
        mSineEffectId = -1;
    }
    InitHapticEffects();
    mAutoRearmCount++;
}

void WheelManager::CloseJoystick() {
    if (mHaptic) {
        if (mConstantEffectId != -1) {
            SDL_HapticDestroyEffect(mHaptic, mConstantEffectId);
            mConstantEffectId = -1;
        }
        if (mSineEffectId != -1) {
            SDL_HapticDestroyEffect(mHaptic, mSineEffectId);
            mSineEffectId = -1;
        }
        SDL_HapticClose(mHaptic);
        mHaptic = nullptr;
        mHapticRumbleSupported = false;
    }
    if (mJoystick) {
        SDL_JoystickClose(mJoystick);
        mJoystick = nullptr;
        mJoystickIndex = -1;
    }
}

void WheelManager::UpdateFFB() {
    if (!mHaptic) return;

    // DirectInput safety monitor: check status periodically (~1 Hz), not every single frame!
    static int sStatusCheckCounter = 0;
    if (++sStatusCheckCounter >= 60) {
        sStatusCheckCounter = 0;
        if (mConstantEffectId != -1) {
            int status = SDL_HapticGetEffectStatus(mHaptic, mConstantEffectId);
            mLastFFBEffectStatus = status;
            if (status == 0) {
                SPDLOG_WARN("Constant FFB effect was stopped by driver; restarting with INFINITY iterations.");
                SDL_HapticRunEffect(mHaptic, mConstantEffectId, SDL_HAPTIC_INFINITY);
            }
        }
    }

    int16_t constantForce = 0;
    int16_t sineMagnitude = 0;
    int16_t softLockForce = 0;

    // Direct Drive Soft Lock / Rotation Limit Bumpstop
    if (mSteeringAxis != -1 && mJoystick) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mSteeringAxis);
        int32_t centered = (int32_t)raw - (int32_t)mSteeringCenter;

        float halfRange = (float)mHardwareDOR * 0.5f;
        if (halfRange < 1.0f) halfRange = 1.0f;
        mCurrentAngleDeg = ((float)centered / 32768.0f) * halfRange;

        if (mSoftLockEnabled) {
            float limitRatio = std::clamp(mSoftLockAngle / halfRange, 0.05f, 1.0f);
            int32_t rawLimit = (int32_t)(32767.0f * limitRatio);

            if (abs(centered) > rawLimit) {
                int32_t overshoot = abs(centered) - rawLimit;
                int32_t remaining = std::max(1, 32767 - rawLimit);
                float penetration = (float)overshoot / (float)remaining;

                // Progressive direct drive bumpstop:
                // Start with a noticeable 30% barrier, ramping smoothly up to 100% over initial penetration
                float ramp = std::min(1.0f, penetration * 4.0f);
                float wallRatio = (0.30f + 0.70f * ramp) * mSoftLockStiffness;
                if (wallRatio > 1.0f) wallRatio = 1.0f;

                int16_t force = (int16_t)(32767.0f * wallRatio);
                // Base direction: if turned right (positive centered), push left (negative force)
                softLockForce = (centered > 0) ? -force : force;
            }
        }
    }
    mLastSoftLockForce = softLockForce;

    // Check if the player is in an active race and in control
    Player* p = &gPlayers[0]; // Assuming local VR player is P1
    bool isRaceActive = (gGamestate == RACING) &&
                        (gDemoMode == DEMO_MODE_INACTIVE) &&
                        (gRaceState < RACE_CALCULATE_RANKS) &&
                        ((p->type & PLAYER_CINEMATIC_MODE) == 0);

    if (!isRaceActive) { // In menus, pause, title screen, demo/attract mode, or after crossing finish line
        constantForce = softLockForce;
        mLastCenteringForce = 0;
        mLastLateralForce = 0;
        mLastRumbleForce = 0;
        mIsOffroadActive = false;

        // Support test triggers while in menus
        if (mTestJoltFrames > 0) {
            constantForce += 28000;
            mTestJoltFrames--;
        }
        mLastJoltForce = (mTestJoltFrames > 0) ? 28000 : 0;
        mJoltFramesRemaining = mTestJoltFrames;

        if (mTestShakeActive) {
            static int sMenuShakeStep = 0;
            sMenuShakeStep++;
            float angle = (float)sMenuShakeStep * (2.0f * 3.14159265f / 3.0f);
            constantForce += (int16_t)(sinf(angle) * 24000.0f);
            mIsSpinoutActive = true;
            mLastSineMagnitude = 24000;
        } else if (mTestJoltFrames <= 0) {
            mIsSpinoutActive = false;
            mLastSineMagnitude = 0;
        }
    } else { // In active race
        mLastPlayerSpeed = p->speed;
        mLastPlayerEffects = p->effects;
        mLastPlayerTriggers = p->triggers;

        static f32 lastSpeed = 0.0f;
        static int sJoltFrames = 0;
        static int16_t sJoltForce = 0;
        static bool sWasHit = false;

        // 1. Dynamic Auto-Centering (Caster trail based on speed and wheel deflection)
        // Center deadband eliminates limit-cycle flutter / hunting around zero on direct drive wheels
        int16_t centeringForce = 0;
        if (mFFBEnableCentering && mSteeringAxis != -1 && p->speed > 0.3f) {
            int16_t raw = SDL_JoystickGetAxis(mJoystick, mSteeringAxis);
            int32_t centered = (int32_t)raw - (int32_t)mSteeringCenter;
            const int32_t kCenterDeadband = 350; // ~1 deg deadband prevents direct drive oscillation
            if (abs(centered) > kCenterDeadband) {
                int32_t active = (centered > 0) ? (centered - kCenterDeadband) : (centered + kCenterDeadband);
                float deflRatio = (float)active / (32767.0f - kCenterDeadband);
                float speedFactor = std::clamp(p->speed / 7.5f, 0.0f, 1.0f);
                centeringForce = (int16_t)(-deflRatio * 32767.0f * speedFactor * 0.40f);
            }
        }
        mLastCenteringForce = centeringForce;
        constantForce += centeringForce;

        // 2. Lateral G (Drifting tire scrub & continuous yaw cornering load)
        int16_t lateralForce = 0;
        if (mFFBEnableLateral) {
            if (p->effects & DRIFTING_EFFECT) {
                // Drifting: lateral tire scrub resisting the slide
                int driftDir = 0;
                if (p->unk_0C0 > 50) driftDir = 1;
                else if (p->unk_0C0 < -50) driftDir = -1;
                lateralForce = (int16_t)(driftDir * 12000);
            } else if (abs(p->unk_078) > 10 && p->speed > 1.0f) {
                // Smooth continuous yaw rate cornering load (no step discontinuities)
                float yawMag = (float)abs(p->unk_078);
                float smoothTurn = (p->unk_078 > 0 ? 1.0f : -1.0f) * std::clamp((yawMag - 10.0f) / 130.0f, 0.0f, 1.0f);
                lateralForce = (int16_t)(-smoothTurn * 6000.0f);
            }
        }
        mLastLateralForce = lateralForce;
        constantForce += lateralForce;

        // 3. Impact & Collision Jolts (Wall hits, shells, bombs, lightning, speed drops)
        bool wallHit = ((p->unk_046 & 0x20) != 0 || 
                        p->collision.surfaceDistance[0] < -0.2f || 
                        p->collision.surfaceDistance[1] < -0.2f) && (p->speed > 0.8f);
        
        // Item hits: green/red/blue shells, explosions, stars, lightning, squish, crash
        bool itemHit = ((p->effects & (0x400 | HIT_BY_ITEM_EFFECT | 0x01000000 | LIGHTNING_EFFECT | HIT_EFFECT)) != 0) ||
                       ((p->kartGraphics & CRASH) != 0);

        bool speedDropHit = (lastSpeed > 2.0f && (lastSpeed - p->speed) > 1.4f);

        static bool sWasWallHit = false;
        static bool sWasItemHit = false;

        bool newHit = false;
        int16_t impactPunch = 0;

        if (wallHit && !sWasWallHit) {
            newHit = true;
            // Directional wall kick: deflect wheel away from the wall!
            if (p->collision.surfaceDistance[0] < -0.2f) {
                impactPunch = -26000; // Hit wall on right -> kick left
            } else if (p->collision.surfaceDistance[1] < -0.2f) {
                impactPunch = 26000;  // Hit wall on left -> kick right
            } else {
                impactPunch = (rand() % 2 == 0) ? 26000 : -26000;
            }
        } else if (itemHit && !sWasItemHit) {
            newHit = true;
            impactPunch = (rand() % 2 == 0) ? 28000 : -28000;
        } else if (speedDropHit && sJoltFrames <= 0) {
            newHit = true;
            impactPunch = (rand() % 2 == 0) ? 26000 : -26000;
        }

        sWasWallHit = wallHit;
        sWasItemHit = itemHit;

        if (newHit && sJoltFrames <= 0) {
            sJoltFrames = 8; // ~130ms punch
            sJoltForce = impactPunch;
        }

        // Test trigger from UI button
        if (mTestJoltFrames > 0) {
            sJoltFrames = mTestJoltFrames;
            sJoltForce = 28000;
            mTestJoltFrames = 0;
        }

        if (sJoltFrames > 0) {
            if (mFFBEnableCollisionJolts) {
                constantForce = sJoltForce;
            }
            sJoltFrames--;
        }
        lastSpeed = p->speed;
        mLastJoltForce = (mFFBEnableCollisionJolts && sJoltFrames > 0) ? sJoltForce : 0;
        mJoltFramesRemaining = sJoltFrames;

        // 4. Spinout & Airborne Item-Hit Tumble Shake (20 Hz synthesized wave in constant force)
        bool isSpinout = ((p->kartProps & DRIVING_SPINOUT) != 0) || 
                         ((p->effects & 0x80) != 0) || 
                         ((p->effects & 0x40) != 0) ||
                         ((p->effects & 0x20000) != 0);

        bool airborneTumble = ((p->effects & (0x400 | HIT_BY_ITEM_EFFECT | 0x01000000 | HIT_EFFECT)) != 0) ||
                              ((p->kartGraphics & CRASH) != 0);

        bool shouldShake = isSpinout || airborneTumble || mTestShakeActive;
        mIsSpinoutActive = shouldShake;
        int16_t spinoutForce = 0;
        static int sSpinoutStep = 0;
        if (mFFBEnableSpinoutShake && shouldShake) {
            sSpinoutStep++;
            float angle = (float)sSpinoutStep * (2.0f * 3.14159265f / 3.0f); // 20 Hz at 60 FPS
            spinoutForce = (int16_t)(sinf(angle) * 24000.0f);
        }
        mLastSineMagnitude = (mFFBEnableSpinoutShake && shouldShake) ? 24000 : 0;
        constantForce += spinoutForce;

        // 5. Rumble (Offroad terrain texture buzz synthesized into constant force)
        bool offroad = (p->surfaceType == GRASS || p->surfaceType == SAND_OFFROAD || 
                        p->surfaceType == SNOW_OFFROAD || p->surfaceType == DIRT_OFFROAD || 
                        p->surfaceType == OUT_OF_BOUNDS);
        if (!offroad) {
            for (int t = 0; t < 4; t++) {
                uint8_t st = p->tyres[t].surfaceType;
                if (st == GRASS || st == SAND_OFFROAD || st == SNOW_OFFROAD || st == DIRT_OFFROAD || st == OUT_OF_BOUNDS) {
                    offroad = true;
                    break;
                }
            }
        }
        mIsOffroadActive = offroad;

        int16_t rumbleForce = 0;
        static int sRumbleStep = 0;
        if (mFFBEnableOffroadRumble && offroad && p->speed > 0.5f) {
            sRumbleStep++;
            float intensity = std::clamp(p->speed / 6.0f, 0.35f, 1.0f);
            int16_t rumbleMag = (int16_t)(intensity * 4500.0f);
            rumbleForce = (sRumbleStep % 2 == 0) ? rumbleMag : -rumbleMag;
        }
        mLastRumbleForce = rumbleForce;
        constantForce += rumbleForce;

        static float sFilteredForce = 0.0f;
        if (!mFFBEnabled) {
            constantForce = 0;
            sFilteredForce = 0.0f;
        } else {
            // Apply FFB Master Gain to gameplay forces
            float gainMult = std::clamp((float)mFFBMasterGain / 100.0f, 0.0f, 1.0f);
            constantForce = (int16_t)(constantForce * gainMult);

            // First-order low pass filter to eliminate 60 Hz frame-to-frame torque ripple on Direct Drive
            sFilteredForce = sFilteredForce * 0.65f + (float)constantForce * 0.35f;
            constantForce = (int16_t)sFilteredForce;
        }

        if (mSoftLockEnabled && softLockForce != 0) {
            // Soft lock wall takes absolute priority over gameplay forces and is unfiltered
            constantForce = softLockForce;
            sFilteredForce = (float)softLockForce;
            sJoltFrames = 0;
        }
    }

    // Invert force direction if configured (needed for wheels whose motor coordinate is flipped)
    if (mFFBInvert) {
        constantForce = -constantForce;
    }

    mLastCommandedForce = constantForce;

    // Update Constant Effect
    if (mConstantEffectId != -1) {
        SDL_HapticEffect effect;
        memset(&effect, 0, sizeof(SDL_HapticEffect));
        effect.type = SDL_HAPTIC_CONSTANT;
        effect.constant.direction.type = mConstantDirectionType;
        effect.constant.direction.dir[0] = 1;
        effect.constant.level = std::max(-32768, std::min(32767, (int)constantForce));
        effect.constant.length = SDL_HAPTIC_INFINITY;
        mLastFFBUpdateResult = SDL_HapticUpdateEffect(mHaptic, mConstantEffectId, &effect);
        if (mLastFFBUpdateResult != 0) {
            mLastFFBError = SDL_GetError();
            mConsecutiveFFBErrors++;
            if (mConsecutiveFFBErrors == 1 || (mConsecutiveFFBErrors % 120 == 0)) {
                SPDLOG_ERROR("DirectInput FFB update failed: {} (consecutive failures: {})", 
                    mLastFFBError.empty() ? "Unknown" : mLastFFBError, mConsecutiveFFBErrors);
            }
            // Auto-rearm / self-healing: if failed for 10 consecutive frames (~160ms), automatically recreate the effect
            if (mConsecutiveFFBErrors >= 10) {
                SPDLOG_WARN("FFB failed for 10 frames - performing auto-rearm recovery...");
                ReInitHapticEffects();
                mConsecutiveFFBErrors = 0;
            } else {
                SDL_HapticRunEffect(mHaptic, mConstantEffectId, SDL_HAPTIC_INFINITY);
            }
        } else {
            if (mConsecutiveFFBErrors > 0) {
                SPDLOG_INFO("FFB recovered successfully after {} failed frames.", mConsecutiveFFBErrors);
            }
            mConsecutiveFFBErrors = 0;
            mLastFFBError.clear();
        }
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

        // Apply Saturation (Steering Lock / Soft Lock alignment)
        float saturation = mSteeringSaturation;
        if (mSoftLockEnabled) {
            float halfRange = (float)mHardwareDOR * 0.5f;
            if (halfRange > 0.0f) {
                float softLockRatio = mSoftLockAngle / halfRange;
                saturation = std::min(saturation, softLockRatio);
            }
        }
        if (saturation < 0.01f) saturation = 0.01f;

        normalized /= saturation;
        if (normalized > 1.0f) normalized = 1.0f;
        if (normalized < -1.0f) normalized = -1.0f;
        
        // Deadzone
        if (abs(normalized) < mSteeringDeadzone) {
            normalized = 0.0f;
        } else {
            normalized = (normalized > 0 ? 1.0f : -1.0f) * (abs(normalized) - mSteeringDeadzone) / (1.0f - mSteeringDeadzone);
        }
        
        // Linearity & S-Curve Blend
        if (normalized != 0.0f) {
            float absNorm = abs(normalized);
            
            // Standard Linearity (Gamma)
            float linearVal = pow(absNorm, mSteeringLinearity);
            
            // Arcade S-Curve: smoothstep 3x^2 - 2x^3
            float sCurveVal = absNorm * absNorm * (3.0f - 2.0f * absNorm);

            // Blend between them based on mSteeringSCurve (0.0 to 1.0)
            float finalVal = (linearVal * (1.0f - mSteeringSCurve)) + (sCurveVal * mSteeringSCurve);

            normalized = (normalized > 0 ? 1.0f : -1.0f) * finalVal;
        }
        
        // Sensitivity
        normalized *= mSteeringSensitivity;
        
        // Clamp to N64 range
        if (normalized > 1.0f) normalized = 1.0f;
        if (normalized < -1.0f) normalized = -1.0f;
        
        // Store native high-res steer for direct game physics use (Approach B)
        mNativeSteer = normalized;
        
        int8_t wheel_stick_x = (int8_t)(normalized * 127.0f);

        if (mCombineInputs) {
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
    mNativeThrottle = 0.0f;
    if (mThrottleAxis != -1) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mThrottleAxis);
        float val = (float)raw / 32767.0f;
        if (mThrottleInvert) val = -val;
        // Normalize typical pedal range [-1, 1] -> [0, 1] if needed, or clamp [0, 1]
        mNativeThrottle = std::clamp((val + 1.0f) * 0.5f, 0.0f, 1.0f);
        if (val > mThrottleThreshold) pad->button |= BTN_A;
    } else if (pad->button & BTN_A) {
        mNativeThrottle = 1.0f;
    }

    // Brake (B button)
    mNativeBrake = 0.0f;
    if (mBrakeAxis != -1) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mBrakeAxis);
        float val = (float)raw / 32767.0f;
        if (mBrakeInvert) val = -val;
        mNativeBrake = std::clamp((val + 1.0f) * 0.5f, 0.0f, 1.0f);
        if (val > mBrakeThreshold) pad->button |= BTN_B;
    } else if (pad->button & BTN_B) {
        mNativeBrake = 1.0f;
    }
    
    // Drift (R button)
    if (mDriftAxis != -1) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mDriftAxis);
        float val = (float)raw / 32767.0f;
        if (mDriftInvert) val = -val;
        if (val > mDriftThreshold) pad->button |= BTN_R;
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

    UpdateFFB();
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

    if (ImGui::Checkbox("Combine Wheel & Keyboard Inputs", &mCombineInputs)) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("When enabled, the game will use whichever input (wheel or keyboard) has the stronger deflection. If disabled, the wheel completely overrides keyboard steering.");

    bool nativeSteer = CVarGetInteger("gWheel.NativeSteer", 1);
    if (ImGui::Checkbox("Native Steering Physics", &nativeSteer)) {
        CVarSetInteger("gWheel.NativeSteer", nativeSteer);
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("When enabled, the wheel's full resolution drives kart steering directly with sim-like physics.\nWhen disabled, the wheel simulates an N64 analog stick (legacy mode).");

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
            if (ImGui::SliderFloat("Saturation (Steering Lock)", &mSteeringSaturation, 0.1f, 1.0f, "%.2f")) SaveSettings();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower values mean less physical wheel rotation is needed to turn 100%. (e.g. 0.5 = half turn is full lock)");
            
            if (ImGui::SliderFloat("Linearity", &mSteeringLinearity, 0.5f, 3.0f, "%.2f")) SaveSettings();
            if (ImGui::SliderFloat("Arcade S-Curve Blend", &mSteeringSCurve, 0.0f, 1.0f, "%.2f")) SaveSettings();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0.0 = Linear/Gamma, 1.0 = Snappy Arcade S-Curve (Gentle center, aggressive turn).");

            ImGui::Separator();
            ImGui::TextDisabled("Note: Force feedback, soft lock, and haptics are configured in the dedicated 'Haptics' menu pane.");
            ImGui::Unindent();
        }

        DrawAxisMapping("Throttle", mThrottleAxis, mThrottleInvert, 1);
        if (mThrottleAxis != -1) {
            ImGui::Indent();
            if (ImGui::SliderFloat("Throttle Threshold", &mThrottleThreshold, 0.0f, 1.0f, "%.2f")) SaveSettings();
            ImGui::Unindent();
        }

        DrawAxisMapping("Brake", mBrakeAxis, mBrakeInvert, 2);
        if (mBrakeAxis != -1) {
            ImGui::Indent();
            if (ImGui::SliderFloat("Brake Threshold", &mBrakeThreshold, 0.0f, 1.0f, "%.2f")) SaveSettings();
            ImGui::Unindent();
        }

        DrawAxisMapping("Drift (R)", mDriftAxis, mDriftInvert, 3);
        if (mDriftAxis != -1) {
            ImGui::Indent();
            if (ImGui::SliderFloat("Drift Threshold", &mDriftThreshold, 0.0f, 1.0f, "%.2f")) SaveSettings();
            ImGui::Unindent();
        }

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

void WheelManager::DrawHapticsSettings() {
    // 1. Device and Motor Status Header
    if (mJoystick) {
        ImGui::Text("Active Device: %s", SDL_JoystickName(mJoystick));
    } else {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), ICON_FA_EXCLAMATION_TRIANGLE " No Racing Wheel Detected");
    }

    if (mHaptic) {
        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.35f, 1.0f), 
            ICON_FA_CHECK_CIRCLE " Force Feedback Motor: Ready (%s)",
            (mConstantDirectionType == SDL_HAPTIC_STEERING_AXIS) ? "STEERING_AXIS" : "CARTESIAN");
    } else if (mJoystick) {
        if (!mHapticErrorStr.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(%s)", mHapticErrorStr.c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(DirectInput FFB Hardware Not Supported)");
        }
    }

    ImGui::Separator();

    // 2. Master FFB Controls
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Master Force Feedback Controls:");
    if (ImGui::Checkbox("Enable Force Feedback (Master)", &mFFBEnabled)) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Master switch to enable or disable all force feedback output to the wheel.");

    if (mFFBEnabled) {
        if (ImGui::SliderInt("Force Feedback Master Gain", &mFFBMasterGain, 0, 100, "%d%%")) {
            if (mHaptic) SDL_HapticSetGain(mHaptic, mFFBMasterGain);
            SaveSettings();
        }

        if (ImGui::Checkbox("Invert Force Feedback Direction", &mFFBInvert)) {
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Check this if the wheel pushes outward into turns instead of resisting back toward center.");

        ImGui::SameLine();
        if (ImGui::Button("Re-arm / Restart FFB")) {
            ReInitHapticEffects();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Destroys and recreates the DirectInput force feedback effects if the motor stopped responding.");
    }

    ImGui::Separator();

    // 3. Individual Haptic Effects Enable / Disable (Central testing location)
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Individual Haptic Effects (Enable / Disable):");
    ImGui::TextDisabled("Toggle specific effects to isolate, test, and tune each force component individually.");

    if (ImGui::Checkbox("Enable Auto-Centering Force", &mFFBEnableCentering)) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Simulates caster trail: wheel dynamically pulls toward center with increasing kart speed and deflection.");

    if (ImGui::Checkbox("Enable Lateral Load (Cornering & Drift)", &mFFBEnableLateral)) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Provides continuous yaw resistance during turns and lateral tire scrub counter-force when drifting.");

    if (ImGui::Checkbox("Enable Collision & Impact Jolts", &mFFBEnableCollisionJolts)) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Directional force punches when colliding with track walls, shells, banana slips, lightning, and heavy speed drops.");

    if (ImGui::Checkbox("Enable Offroad Terrain Texture / Rumble", &mFFBEnableOffroadRumble)) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Synthesizes surface buzz and vibration when tires roll over grass, dirt, sand, or snow offroad.");

    if (ImGui::Checkbox("Enable Spinout & Airborne Tumble Shake", &mFFBEnableSpinoutShake)) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("20 Hz oscillation wave when spinning out or tumbling through the air after being hit.");

    // Direct Drive Soft Lock Bumpstop Wall
    if (ImGui::Checkbox("Enable FFB Soft Lock Wall (Direct Drive)", &mSoftLockEnabled)) {
        SaveSettings();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Uses Direct Drive motor force to create a physical hard-stop wall at the target rotation angle.");

    if (mSoftLockEnabled) {
        ImGui::Indent();
        if (ImGui::InputInt("Wheel Hardware Range (DOR)", &mHardwareDOR, 10, 90)) {
            if (mHardwareDOR < 90) mHardwareDOR = 90;
            if (mHardwareDOR > 2520) mHardwareDOR = 2520;
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Type or step the exact total rotation angle set in your wheel driver (e.g. 900 or 1080).");

        ImGui::SameLine();
        if (ImGui::SmallButton("900 deg")) { mHardwareDOR = 900; SaveSettings(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("1080 deg")) { mHardwareDOR = 1080; SaveSettings(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("540 deg")) { mHardwareDOR = 540; SaveSettings(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("360 deg")) { mHardwareDOR = 360; SaveSettings(); }

        if (ImGui::InputFloat("Soft Lock Angle (Each Side)", &mSoftLockAngle, 5.0f, 15.0f, "%.0f deg")) {
            if (mSoftLockAngle < 15.0f) mSoftLockAngle = 15.0f;
            if (mSoftLockAngle > (float)mHardwareDOR * 0.5f) mSoftLockAngle = (float)mHardwareDOR * 0.5f;
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Target angle from center before hitting the motor bumpstop. 90 deg = 180 deg total lock-to-lock.");

        ImGui::SameLine();
        if (ImGui::SmallButton("+/-90 deg")) { mSoftLockAngle = 90.0f; SaveSettings(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("+/-180 deg")) { mSoftLockAngle = 180.0f; SaveSettings(); }

        if (ImGui::SliderFloat("Wall Stiffness", &mSoftLockStiffness, 0.2f, 2.0f, "%.2f")) {
            SaveSettings();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How strongly the motor pushes back when hitting the limit.");
        ImGui::Unindent();
    }

    // Telemetry gear shift pulse
    bool itemFeedback = TelemetryManager::GetInstance()->IsItemFeedbackEnabled();
    if (ImGui::Checkbox("Enable Gear-Shift & Boost Item Feedback (SimHub / ShakeIt)", &itemFeedback)) {
        TelemetryManager::GetInstance()->SetItemFeedbackEnabled(itemFeedback);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transmits gear shift events and acceleration kicks to SimHub motion rigs and bass shakers on item activation.");

    ImGui::Separator();

    // 4. Interactive Test Triggers
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Interactive Haptic Tests:");
    if (ImGui::Button("Test Collision Jolt")) {
        mTestJoltFrames = 8;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Manually triggers an impact jolt so you can feel the collision force.");

    ImGui::SameLine();
    if (ImGui::Button(mTestShakeActive ? "Stop 20Hz Shake" : "Test 20Hz Shake")) {
        mTestShakeActive = !mTestShakeActive;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggles the 20Hz spinout / airborne tumble shake effect.");
    if (mTestShakeActive) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "(Active)");
    }

    ImGui::SameLine();
    if (ImGui::Button("Test Item Boost Feedback")) {
        TelemetryManager::GetInstance()->TriggerItemFeedback(ITEM_MUSHROOM);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sends a boost gear-shift pulse to SimHub ShakeIt / motion telemetry.");

    ImGui::Separator();

    // 5. Live Diagnostics & Telemetry
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Wheel Telemetry & Diagnostics:");

    if (mJoystick && mSteeringAxis != -1) {
        int16_t raw = SDL_JoystickGetAxis(mJoystick, mSteeringAxis);
        float currentAngle = mCurrentAngleDeg;
        float halfDOR = (float)mHardwareDOR * 0.5f;
        if (halfDOR < 1.0f) halfDOR = 1.0f;
        
        float normAngle = std::clamp((currentAngle + halfDOR) / (halfDOR * 2.0f), 0.0f, 1.0f);
        ImGui::ProgressBar(normAngle, ImVec2(-1, 0), StringHelper::Sprintf("Angle: %+.1f deg (Raw: %d)", currentAngle, raw).c_str());

        if (mSoftLockEnabled) {
            if (abs(currentAngle) > mSoftLockAngle) {
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f),
                    ICON_FA_EXCLAMATION_TRIANGLE " BUMPSTOP ENGAGED: %s by %.1f deg",
                    (currentAngle > 0) ? "RIGHT" : "LEFT", abs(currentAngle) - mSoftLockAngle);
            } else {
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.35f, 1.0f), 
                    "Within Safe Range (+-%.0f deg)", mSoftLockAngle);
            }
        }
    }

    ImGui::Text("Commanded FFB Force: %d (Soft Lock: %d)", mLastCommandedForce, mLastSoftLockForce);
    if (ImGui::TreeNodeEx("Active Forces Breakdown", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BulletText("Auto-Centering: %d (%s)", mLastCenteringForce, mFFBEnableCentering ? "Enabled" : "Disabled");
        ImGui::BulletText("Lateral Load (Drift/Turn): %d (%s)", mLastLateralForce, mFFBEnableLateral ? "Enabled" : "Disabled");
        ImGui::BulletText("Collision Jolt: %d (%s, %d frames remaining)", mLastJoltForce, mFFBEnableCollisionJolts ? "Enabled" : "Disabled", mJoltFramesRemaining);
        ImGui::BulletText("Offroad Terrain Texture: %d (%s, %s)", mLastRumbleForce, mFFBEnableOffroadRumble ? "Enabled" : "Disabled", mIsOffroadActive ? "ACTIVE" : "Idle");
        ImGui::BulletText("Spinout 20Hz Shake: %s (%s, Magnitude: %d)", mIsSpinoutActive ? "ACTIVE" : "Idle", mFFBEnableSpinoutShake ? "Enabled" : "Disabled", mLastSineMagnitude);
        ImGui::BulletText("Player Speed: %.2f | Effects: 0x%08X", mLastPlayerSpeed, mLastPlayerEffects);
        ImGui::TreePop();
    }

    const char* statusStr = "Not Created";
    if (mConstantEffectId != -1) {
        if (mLastFFBEffectStatus == 1) statusStr = "Playing";
        else if (mLastFFBEffectStatus == 0) statusStr = "Stopped (Auto-restarting)";
        else statusStr = "Status Query Error";
    }
    ImGui::Text("FFB Motor Status: %s (Type: %s)", statusStr, 
        (mConstantDirectionType == SDL_HAPTIC_STEERING_AXIS) ? "STEERING_AXIS" : "CARTESIAN");

    if (mLastFFBUpdateResult == 0) {
        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.35f, 1.0f), "DirectInput Update: OK (Auto-Rearms: %d)", mAutoRearmCount);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "DirectInput Update Error: %s (Failures: %d, Auto-Rearms: %d)", 
            mLastFFBError.empty() ? "Failed" : mLastFFBError.c_str(), mConsecutiveFFBErrors, mAutoRearmCount);
    }
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

void WheelManager_DrawHapticsSettings() {
    WheelManager::GetInstance()->DrawHapticsSettings();
}

float WheelManager_GetNativeSteer() {
    WheelManager* wm = WheelManager::GetInstance();
    if (wm->IsEnabled()) {
        return wm->GetNativeSteer();
    }
    return 0.0f;
}

float WheelManager_GetThrottle() {
    WheelManager* wm = WheelManager::GetInstance();
    if (wm->IsEnabled()) {
        return wm->GetThrottle();
    }
    return 0.0f;
}

float WheelManager_GetBrake() {
    WheelManager* wm = WheelManager::GetInstance();
    if (wm->IsEnabled()) {
        return wm->GetBrake();
    }
    return 0.0f;
}

int WheelManager_IsNativeSteerActive() {
    WheelManager* wm = WheelManager::GetInstance();
    return (wm->IsEnabled() && CVarGetInteger("gWheel.NativeSteer", 1)) ? 1 : 0;
}
}
