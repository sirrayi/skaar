/*
===========================================================================
DEV_ASSERT: soft assert for Skaar code. In dev builds (SKAAR_DEV) a failed
check logs "ASSERT file:line message" plus a backtrace, shows an in-game
toast, and keeps running. In release builds it compiles to nothing (the
condition is not evaluated).

Usable from the engine and from C/C++ game modules: each links dev_assert.c,
which reports through that module's Com_Printf.

This file is part of Skaar, a fork of ioquake3. GPLv2 or later.
===========================================================================
*/
#ifndef DEV_ASSERT_H
#define DEV_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

void Dev_AssertFailed( const char *file, int line, const char *expr, const char *msg );

#ifdef __cplusplus
}
#endif

#ifdef SKAAR_DEV
#define DEV_ASSERT( cond, msg ) \
	do { if ( !( cond ) ) Dev_AssertFailed( __FILE__, __LINE__, #cond, ( msg ) ); } while ( 0 )
#else
#define DEV_ASSERT( cond, msg ) ( (void)0 )
#endif

#endif
