/*
===========================================================================
Skaar developer diagnostics (engine side). See docs/dev-loop.md.

This file is part of Skaar, a fork of ioquake3. GPLv2 or later.
===========================================================================
*/
#ifndef DEV_H
#define DEV_H

#include "q_shared.h"

typedef enum {
	DEV_NOTE_WARNING,
	DEV_NOTE_ERROR		// ERROR, FATAL, ASSERT, sanitizer
} devNoteLevel_t;

typedef struct {
	int				time;		// Sys_Milliseconds when printed
	devNoteLevel_t	level;
	char			text[192];
} devNote_t;

typedef struct {
	float	fps;			// frames counted over the last second
	float	frameMsec;		// smoothed frame interval
	float	frameP50;		// frame interval percentiles over the last second, msec
	float	frameP99;
	float	tickRate;		// server ticks counted over the last second (0 if no local server)
	float	inputAgeMsec;	// newest mouse input -> end of the frame that used it (buffer swap), smoothed
	float	inputAgeMaxMsec;	// worst over the last second
	const char	*mouseSource;	// "raw (GCMouse)", "SDL (accelerated on macOS)", ...
} devStats_t;

void		Dev_Init( void );

// Called from Com_Printf for every message. Never prints.
void		Dev_OnPrint( const char *msg );

// Fills out[] with up to max notes, oldest first. maxAgeMsec <= 0 means any age.
int			Dev_RecentNotes( devNote_t *out, int max, int maxAgeMsec );

// Last ERR_DROP message, if it happened within maxAgeMsec and after the last map load
const char	*Dev_RecentError( int maxAgeMsec );

// Frame timing from Com_Frame, in microseconds. Logs "spike" lines.
void		Dev_FrameEnd( int eventsUsec, int serverUsec, int clientUsec );
void		Dev_SuppressSpikes( int frames );

// Input timing: clock time (Dev_Microseconds) of the newest mouse motion used this frame
void		Dev_NoteInput( long long usec );
void		Dev_SetMouseSource( const char *source );

// From SV_Frame: ticks run this frame and the backlog (msec) before running them
void		Dev_ServerTicks( int ticksRun, int backlogMsec, int frameMsec );

void		Dev_GetStats( devStats_t *stats );

// Called when a server map finishes loading; writes logs/last-session.cfg
void		Dev_OnMapLoaded( const char *mapname );

void		Dev_SetGpuInfo( const char *info );
const char	*Dev_GpuInfo( void );

// Directory for session logs and crash reports (dev_logdir, else <homepath>/logs)
const char	*Dev_LogDir( void );
// Path of the human-readable session log (dev_logpath, else qconsole.log)
const char	*Dev_LogPath( void );

long long	Dev_Microseconds( void );

#endif
