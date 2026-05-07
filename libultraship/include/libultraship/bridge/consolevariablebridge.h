#ifndef CONSOLE_VARIABLE_BRIDGE_H
#define CONSOLE_VARIABLE_BRIDGE_H

#include "stdint.h"
#include "libultraship/color.h"

#ifdef __cplusplus
#include <memory>
#include "ship/config/ConsoleVariable.h"
extern "C" {
#endif

int32_t CVarGetInteger(const char* name, int32_t defaultValue);
float CVarGetFloat(const char* name, float defaultValue);
const char* CVarGetString(const char* name, const char* defaultValue);
Color_RGBA8 CVarGetColor(const char* name, Color_RGBA8 defaultValue);
Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 defaultValue);

void CVarSetInteger(const char* name, int32_t value);
void CVarSetFloat(const char* name, float value);
void CVarSetString(const char* name, const char* value);
void CVarSetColor(const char* name, Color_RGBA8 value);
void CVarSetColor24(const char* name, Color_RGB8 value);

void CVarRegisterInteger(const char* name, int32_t defaultValue);
void CVarRegisterFloat(const char* name, float defaultValue);
void CVarRegisterString(const char* name, const char* defaultValue);
void CVarRegisterColor(const char* name, Color_RGBA8 defaultValue);
void CVarRegisterColor24(const char* name, Color_RGB8 defaultValue);

void CVarClear(const char* name);
bool CVarExists(const char* name);
void CVarClearBlock(const char* name);
void CVarCopy(const char* from, const char* to);

void CVarLoad();
void CVarSave();

#ifdef __cplusplus
}

namespace Ship {
    using ::CVarGetInteger;
    using ::CVarGetFloat;
    using ::CVarGetString;
    using ::CVarGetColor;
    using ::CVarGetColor24;
    using ::CVarSetInteger;
    using ::CVarSetFloat;
    using ::CVarSetString;
    using ::CVarSetColor;
    using ::CVarSetColor24;
    using ::CVarRegisterInteger;
    using ::CVarRegisterFloat;
    using ::CVarRegisterString;
    using ::CVarRegisterColor;
    using ::CVarRegisterColor24;
    using ::CVarClear;
    using ::CVarExists;
    using ::CVarClearBlock;
    using ::CVarCopy;
    using ::CVarLoad;
    using ::CVarSave;

    std::shared_ptr<Ship::CVar> CVarGet(const char* name);
}
#endif

#endif
