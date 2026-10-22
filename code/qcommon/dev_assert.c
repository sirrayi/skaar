/*
===========================================================================
DEV_ASSERT reporting. Compiled into the engine and into every game module.

This file is part of Skaar, a fork of ioquake3. GPLv2 or later.
===========================================================================
*/
#include "q_shared.h"
#include "dev_assert.h"

#if defined( __APPLE__ ) || defined( __linux__ )
#include <execinfo.h>
#include <stdlib.h>
#define DEV_HAVE_BACKTRACE
#endif

#define MAX_ASSERT_SITES 64

typedef struct {
	const char	*file;
	int			line;
	int			count;
} assertSite_t;

static assertSite_t assertSites[ MAX_ASSERT_SITES ];

// Strip the absolute build path down to the repo-relative part
static const char *Dev_ShortPath( const char *file ) {
	static const char *roots[] = { "/code/", "/game/", "/tools/", "/tests/" };
	const char *best = NULL;
	size_t i;

	for ( i = 0; i < ARRAY_LEN( roots ); i++ ) {
		const char *p = strstr( file, roots[ i ] );
		if ( p && ( !best || p > best ) ) {
			best = p;
		}
	}
	return best ? best + 1 : file;
}

// Returns how many times this call site has failed, including now
static int Dev_CountSite( const char *file, int line ) {
	int i;

	for ( i = 0; i < MAX_ASSERT_SITES; i++ ) {
		assertSite_t *site = &assertSites[ i ];
		if ( !site->file ) {
			site->file = file;
			site->line = line;
			site->count = 1;
			return 1;
		}
		if ( site->line == line && !strcmp( site->file, file ) ) {
			return ++site->count;
		}
	}
	return 1;	// table full: report every time
}

static qboolean Dev_ShouldReport( int count ) {
	// 1st, 10th, 100th... so a failing assert in a hot loop can't flood the log
	while ( count >= 10 && count % 10 == 0 ) {
		count /= 10;
	}
	return count == 1;
}

void Dev_AssertFailed( const char *file, int line, const char *expr, const char *msg ) {
	int count = Dev_CountSite( file, line );

	if ( !Dev_ShouldReport( count ) ) {
		return;
	}

	Com_Printf( S_COLOR_RED "ASSERT %s:%d %s (%s)%s\n", Dev_ShortPath( file ), line,
		msg ? msg : "", expr, count > 1 ? va( " [x%d]", count ) : "" );

#ifdef DEV_HAVE_BACKTRACE
	if ( count == 1 ) {
		void *frames[ 32 ];
		int n = backtrace( frames, ARRAY_LEN( frames ) );
		char **symbols = backtrace_symbols( frames, n );
		int i;

		if ( symbols ) {
			// skip frame 0 (this function)
			for ( i = 1; i < n; i++ ) {
				Com_Printf( "  #%d %s\n", i - 1, symbols[ i ] );
			}
			free( symbols );
		}
	}
#endif
}
