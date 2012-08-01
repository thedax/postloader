#ifndef _STFDEBUG_
	#define _STFDEBUG_
	
	s32 DebugStart (bool gecko, char *fn);
	void DebugStop (void);
	void Debug (const char *text, ...);
	void Debug_hexdump(void *d, int len);
	void Debug_hexdumplog (void *d, int len);
	void gprintf (const char *format, ...);
#endif