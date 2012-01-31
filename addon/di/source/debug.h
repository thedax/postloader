#ifndef _STFDEBUG_
	#define _STFDEBUG_
	
	s32 DebugStart (bool gecko, char *fn);
	void DebugStop (void);
	void Debug (const char *text, ...);
#endif