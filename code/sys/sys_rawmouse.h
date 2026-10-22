/*
===========================================================================
Raw (unaccelerated) relative mouse motion where SDL 2 doesn't provide it.
macOS 14+: GCMouse (sys_osx_rawmouse.m). Other platforms: SDL's relative mode
is already raw (Win32 raw input, XInput2/Wayland relative pointer).
See docs/feel.md.

This file is part of Skaar, a fork of ioquake3. GPLv2 or later.
===========================================================================
*/
#ifndef SYS_RAWMOUSE_H
#define SYS_RAWMOUSE_H

#include "../qcommon/q_shared.h"

// Starts listening for mice. False if raw input isn't supported here.
qboolean	RawMouse_Init( void );
void		RawMouse_Shutdown( void );

// True once at least one mouse is attached and delivering raw motion
qboolean	RawMouse_Active( void );

// Motion is only accumulated while capturing (the game owns the mouse)
void		RawMouse_SetCapture( qboolean capture );

// Takes the whole counts accumulated since the last call (fractions carry
// over). newestUsec: Dev_Microseconds() clock time of the newest motion, or 0.
void		RawMouse_Consume( int *dx, int *dy, long long *newestUsec );

const char	*RawMouse_Description( void );

#endif
