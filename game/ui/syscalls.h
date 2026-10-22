// UI -> engine calls (uiImport_t in code/ui/ui_public.h).
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

#include <string>

#include "../shared/q3.h"

extern "C" {
#include "renderercommon/tr_types.h"
#include "ui/ui_public.h"
#include "client/keycodes.h"

// called by the reused bg_misc.c; defined in syscalls.cpp
void trap_Cvar_VariableStringBuffer(const char* name, char* buffer, int size);
}

namespace ui::trap {

void Print(const char* text);
[[noreturn]] void Error(const char* text);
int Milliseconds();
void CvarSet(const char* name, const char* value);
std::string CvarString(const char* name);
void CvarRegister(vmCvar_t* cvar, const char* name, const char* value, int flags);
void CvarUpdate(vmCvar_t* cvar);
int Argc();
std::string Argv(int n);
void CmdExecuteText(int when, const char* text);
bool FileRead(const char* path, std::string& out);
qhandle_t R_RegisterShaderNoMip(const char* name);
void R_SetColor(const float* rgba);
void R_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader);
int Key_GetCatcher();
void Key_SetCatcher(int catcher);
void Key_ClearStates();
void GetClientState(uiClientState_t* state);
void GetGlconfig(glconfig_t* config);
std::string GetConfigString(int index);

}  // namespace ui::trap
