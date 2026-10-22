// Bridge to the engine's C headers. The engine, the module ABI and the reused
// Q3 movement code (bg_*.c) are C; Skaar's modules are C++20.
//
// This file is part of Skaar. GPLv2 or later.
#pragma once

extern "C" {
#include "qcommon/q_shared.h"
#include "qcommon/dev_assert.h"
#include "game/bg_public.h"

// q_shared.h declares Com_Printf/Com_Error; modules also provide this one
void QDECL Com_DPrintf(const char* fmt, ...) Q_PRINTF_FUNC(1, 2);
}

#include <cstdarg>
#include <cstdio>

namespace skaar {

constexpr int kMaxPrint = 4096;  // engine's MAXPRINTMSG

// const-correct va(): Q3's takes a char *format, which C++ string literals aren't.
// Rotates through a few static buffers, so a handful of results can be alive at once.
inline const char* Va(const char* fmt, ...) Q_PRINTF_FUNC(1, 2);
inline const char* Va(const char* fmt, ...) {
    static char buffers[4][kMaxPrint];
    static int index;
    char* buf = buffers[index++ & 3];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, kMaxPrint, fmt, ap);
    va_end(ap);
    return buf;
}

// Floats cross the syscall boundary as their int bit pattern (the engine
// reads them back with VMF()), exactly as the C modules' PASSFLOAT does.
inline intptr_t PassFloat(float f) {
    floatint_t fi;
    fi.f = f;
    return fi.i;
}

}  // namespace skaar

// Module entry points. Every module defines both with C linkage.
#define SKAAR_MODULE_EXPORT extern "C" Q_EXPORT
