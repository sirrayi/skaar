// This file is part of Skaar. GPLv2 or later.
#include "syscalls.h"

#include <cstdlib>

using skaar::PassFloat;

namespace {
intptr_t(QDECL* g_syscall)(intptr_t arg, ...) = nullptr;
}

SKAAR_MODULE_EXPORT void dllEntry(intptr_t(QDECL* syscallptr)(intptr_t arg, ...)) { g_syscall = syscallptr; }

extern "C" void trap_Cvar_VariableStringBuffer(const char* name, char* buffer, int size) {
    g_syscall(UI_CVAR_VARIABLESTRINGBUFFER, name, buffer, size);
}

namespace ui::trap {

void Print(const char* text) { g_syscall(UI_PRINT, text); }

void Error(const char* text) {
    g_syscall(UI_ERROR, text);
    std::abort();  // UI_ERROR longjmps out of the module; never reached
}

int Milliseconds() { return static_cast<int>(g_syscall(UI_MILLISECONDS)); }
void CvarSet(const char* name, const char* value) { g_syscall(UI_CVAR_SET, name, value); }

std::string CvarString(const char* name) {
    char buf[MAX_CVAR_VALUE_STRING];
    trap_Cvar_VariableStringBuffer(name, buf, sizeof(buf));
    return buf;
}

void CvarRegister(vmCvar_t* cvar, const char* name, const char* value, int flags) {
    g_syscall(UI_CVAR_REGISTER, cvar, name, value, flags);
}

void CvarUpdate(vmCvar_t* cvar) { g_syscall(UI_CVAR_UPDATE, cvar); }
int Argc() { return static_cast<int>(g_syscall(UI_ARGC)); }

std::string Argv(int n) {
    char buf[MAX_STRING_CHARS];
    g_syscall(UI_ARGV, n, buf, sizeof(buf));
    return buf;
}

void CmdExecuteText(int when, const char* text) { g_syscall(UI_CMD_EXECUTETEXT, when, text); }

bool FileRead(const char* path, std::string& out) {
    fileHandle_t f;
    const int len = static_cast<int>(g_syscall(UI_FS_FOPENFILE, path, &f, FS_READ));
    if (len < 0 || !f) {
        return false;
    }
    out.resize(len);
    g_syscall(UI_FS_READ, out.data(), len, f);
    g_syscall(UI_FS_FCLOSEFILE, f);
    return true;
}

qhandle_t R_RegisterShaderNoMip(const char* name) {
    return static_cast<qhandle_t>(g_syscall(UI_R_REGISTERSHADERNOMIP, name));
}

void R_SetColor(const float* rgba) { g_syscall(UI_R_SETCOLOR, rgba); }

void R_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader) {
    g_syscall(UI_R_DRAWSTRETCHPIC, PassFloat(x), PassFloat(y), PassFloat(w), PassFloat(h), PassFloat(s1),
              PassFloat(t1), PassFloat(s2), PassFloat(t2), shader);
}

int Key_GetCatcher() { return static_cast<int>(g_syscall(UI_KEY_GETCATCHER)); }
void Key_SetCatcher(int catcher) { g_syscall(UI_KEY_SETCATCHER, catcher); }
void Key_ClearStates() { g_syscall(UI_KEY_CLEARSTATES); }
void GetClientState(uiClientState_t* state) { g_syscall(UI_GETCLIENTSTATE, state); }
void GetGlconfig(glconfig_t* config) { g_syscall(UI_GETGLCONFIG, config); }

std::string GetConfigString(int index) {
    char buf[MAX_INFO_STRING];
    if (!g_syscall(UI_GETCONFIGSTRING, index, buf, sizeof(buf))) {
        return "";
    }
    return buf;
}

std::vector<std::string> FileList(const char* dir, const char* extension) {
    static char buf[64 * 1024];
    const int count = static_cast<int>(g_syscall(UI_FS_GETFILELIST, dir, extension, buf, sizeof(buf)));
    std::vector<std::string> out;
    const char* p = buf;
    for (int i = 0; i < count && p < buf + sizeof(buf); i++) {
        out.emplace_back(p);
        p += out.back().size() + 1;
    }
    return out;
}

bool FileWrite(const char* path, const std::string& data) {
    fileHandle_t f;
    g_syscall(UI_FS_FOPENFILE, path, &f, FS_WRITE);
    if (!f) {
        return false;
    }
    g_syscall(UI_FS_WRITE, data.data(), static_cast<int>(data.size()), f);
    g_syscall(UI_FS_FCLOSEFILE, f);
    return true;
}

float CvarValue(const char* name) {
    floatint_t fi;
    fi.i = static_cast<int>(g_syscall(UI_CVAR_VARIABLEVALUE, name));
    return fi.f;
}

sfxHandle_t S_RegisterSound(const char* name) {
    return static_cast<sfxHandle_t>(g_syscall(UI_S_REGISTERSOUND, name, qfalse));
}

void S_StartLocalSound(sfxHandle_t sfx, int channel) { g_syscall(UI_S_STARTLOCALSOUND, sfx, channel); }
void RealTime(qtime_t* t) { g_syscall(UI_REAL_TIME, t); }

std::string KeyName(int key) {
    char buf[64];
    g_syscall(UI_KEY_KEYNUMTOSTRINGBUF, key, buf, sizeof(buf));
    return buf;
}

std::string KeyBinding(int key) {
    char buf[256];
    g_syscall(UI_KEY_GETBINDINGBUF, key, buf, sizeof(buf));
    return buf;
}

void SetBinding(int key, const char* command) { g_syscall(UI_KEY_SETBINDING, key, command); }

}  // namespace ui::trap
