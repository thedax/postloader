#include <gccore.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#include "mem2.h"
#include "globals.h"

#define PRIO_IDLE 8
#define PRIO_HI 32

static mutex_t mutex;

#define SET(a, b) a = b; DCFlushRange(&a, sizeof(a)); ICInvalidateRange(&a, sizeof(a));
//#define SET(a, b) a = b;
#define MAXITEMS 128

#define STACKSIZE	8192
static u8 * threadStack = NULL;
static lwp_t hthread = LWP_THREAD_NULL;

#define NOTFOUNDBUFFSIZE 8192
static char notFoundBuff[NOTFOUNDBUFFSIZE];
static volatile int notFoundBuffLen = 0;

typedef struct 
	{
	char id[256];
	GRRLIB_texImg *cover;
	u8 prio;	// 1 or 0, 
	u32 age;
	}
s_cc; // covercache

static s_cc *cc;

static int age = 0;

#define PAUSE_NO 0
#define PAUSE_REQUEST 1
#define PAUSE_YES 2

static int threadPause = PAUSE_NO;
static volatile int threadStop = 0;
static volatile int threadRunning = 0;
static volatile int cId = 0;
static volatile int update = 0;

#define UPDATE_TP() DCFlushRange(threadPause, sizeof(int))

static void notFound_Init (void)
	{
	*notFoundBuff = '\0';
	notFoundBuffLen = 0;
	}
	
static void notFound_Add (char *path)
	{
	char *p = strrchr (path, '/');
	if (!p) return;
	
	p++;
	
	int l = strlen (p);
	
	if (l + notFoundBuffLen >= NOTFOUNDBUFFSIZE)
		{
		notFound_Init (); // we should do something more clever :P
		}
		
	strcat (notFoundBuff, p);
	strcat (notFoundBuff, ";");
	
	DCFlushRange(&notFoundBuff[notFoundBuffLen], l+1);
	
	notFoundBuffLen += (l+1);
	}

static bool notFound_Find (char *path)
	{
	char *p = strrchr (path, '/');
	if (!p) return false;
	
	p++;
	
	if (strstr (notFoundBuff, p)) // found
		return true;
		
	return false;
	}

static GRRLIB_texImg *MoveTex2Mem2 (GRRLIB_texImg *tex)
	{
	if (!tex) return NULL;
	
	GRRLIB_texImg *newTex = mem2_malloc (sizeof(GRRLIB_texImg));
	if (!newTex) return NULL;
	
	memcpy (newTex, tex, sizeof(GRRLIB_texImg));
	DCFlushRange(newTex, sizeof(GRRLIB_texImg));

	u32 size = tex->w * tex->h * 4;
	newTex->data = mem2_malloc (size);
	
	if (!newTex->data) 
		{
		mem2_free (newTex);
		return NULL;
		}
	
	memcpy (newTex->data, tex->data, size);
	DCFlushRange(newTex->data, size);
	
	GRRLIB_FreeTexture (tex);
	
	return newTex;
	}
	
static void FreeMem2Tex (GRRLIB_texImg *tex)
	{
	if (!tex) return;
	
	DCFlushRange(tex, sizeof(GRRLIB_texImg));
	
	if (tex->data)
		mem2_free(tex->data);

	mem2_free(tex);
	}

static void *thread (void *arg)
	{
	GRRLIB_texImg *tex;

	threadRunning = 1;

	Debug ("covercache thread started");
	
	int doPrio = 0;
	int i;
	int read;
	int currentPrio = PRIO_IDLE;
	
	while (true)
		{
		read = 0;
		
		for (i = 0; i < MAXITEMS; i++)
			{
			if (*cc[i].id != '\0' && !cc[i].cover && ((doPrio == 1 && cc[i].prio == 1) || doPrio == 0))
				{
				read = 1;

				if (currentPrio == PRIO_IDLE) 
					{
					currentPrio = PRIO_HI;
					LWP_SetThreadPriority (hthread, currentPrio);
					}
			
				cId = i;
				tex = GRRLIB_LoadTextureFromFile (cc[i].id);
				
				cc[i].prio = 0;
 				cc[i].cover = MoveTex2Mem2 (tex);
				
				if (!cc[i].cover) 
					{
					notFound_Add (cc[i].id); // add to blacklist
					*cc[i].id = '\0'; // do not try again
					}
				DCFlushRange(&cc[i], sizeof(s_cc));
				update ++;

				cId = -1;
				}
				
			//gprintf ("%d ",*threadPause);
			if (threadPause == PAUSE_REQUEST)
				{
				SET (threadPause, PAUSE_YES);
				
				int tout = 10000;
				while (threadPause == PAUSE_YES && --tout > 0)
						{
						usleep (100);
						LWP_YieldThread();
						}
				if (tout < 0) Debug ("tout on threadPause");
				
				doPrio = 1;
				i = 0;
				}
			
			if (threadStop)
				{
				threadRunning = 0;
				return NULL;
				}
			}
			
		if (read == 0 && currentPrio == PRIO_HI) 
			{
			currentPrio = PRIO_IDLE;
			LWP_SetThreadPriority (hthread, currentPrio);
			}

		usleep (100);
		
		doPrio = 0;
		}
	
	return NULL;
	}

void CoverCache_Pause (bool yes) // return after putting thread in 
	{
	if (!threadRunning) return;
	
	if (yes)
		{
		SET (threadPause, PAUSE_REQUEST);

		int tout = 10000;
		while (threadPause != PAUSE_YES && --tout > 0)
			{
			usleep(100);
			LWP_YieldThread();
			}
		if (tout < 0) Debug ("tout on CoverCache_Pause request");
		}
	else
		{
		threadPause = PAUSE_NO;
		}
	}
	
void CoverCache_Start (void)
	{
	Debug ("CoverCache_Start");
	
	cc = calloc (1, MAXITEMS * sizeof(s_cc));

	mutex = LWP_MUTEX_NULL;
    LWP_MutexInit (&mutex, false);

	threadStack = (u8 *) memalign(32, STACKSIZE);
	LWP_CreateThread (&hthread, thread, NULL, threadStack, STACKSIZE, PRIO_IDLE);
	LWP_ResumeThread(hthread);
	}

void CoverCache_Flush (void)	// empty the cache
	{
	int i;
	int count = 0;
	Debug ("CoverCache_Flush");
	
	CoverCache_Pause (true);
	
	notFound_Init ();
	
	DCFlushRange(cc, MAXITEMS * sizeof(s_cc));
	
	for (i = 0; i < MAXITEMS; i++)
		{
		if (cc[i].cover) 
			{
			FreeMem2Tex (cc[i].cover);
			cc[i].cover = NULL;
			
			count ++;
			}
			
		*cc[i].id = '\0';
		cc[i].age = 0;
		}
	age = 0;
	
	DCFlushRange(cc, MAXITEMS * sizeof(s_cc));

	CoverCache_Pause (false);

	Debug ("CoverCache_Flush: %d covers flushed", count);
	}
	
void CoverCache_Stop (void)
	{
	Debug ("CoverCache_Stop");
	if (threadRunning)
		{
		threadStop = 1;
		
		int i;
		for (i = 0; i < 2000; i++)
			{
			if (threadRunning == 0) break;
			usleep (1000);
			}
		
		if (threadRunning)
			{
			threadStop = 1;
			Debug ("CoverCache_Stop: Warning, thread doesn't respond !");
			}
		else
			{
			LWP_JoinThread (hthread, NULL);
			CoverCache_Flush ();
			free (threadStack);
			free (cc);
			LWP_MutexDestroy (mutex);
			}
		}
	else
		Debug ("CoverCache_Stop: thread was already stopped");
	}
	
GRRLIB_texImg *CoverCache_Get (char *id) // this will return the same text
	{
	if (threadRunning == 0) return NULL;

	int i;
	GRRLIB_texImg *tex = NULL;

	for (i = 0; i < MAXITEMS; i++)
		{
		if (i != cId && *cc[i].id != '\0' && strcmp (id, cc[i].id) == 0)
			{
			tex = cc[i].cover;
			break;
			}
		}

	return tex;
	}
	
GRRLIB_texImg *CoverCache_GetCopy (char *id) // this will return a COPY of the required texture
	{
	return grlib_CreateTexFromTex (CoverCache_Get (id));
	}
	
bool CoverCache_IsUpdated (void) // this will return the same text
	{
	static int lastCheck = 0;
	static u32 mstout = 0;
	
	u32 ms = ticks_to_millisecs(gettime());
	
	if (mstout < ms)
		{
		mstout = ms + 100;
		
		if (lastCheck != update)
			{
			lastCheck = update;
			return true;
			}
		}
	return false;
	}

void CoverCache_Add (char *id, bool priority)
	{
	int i;
	bool found = false;
	
	if (notFound_Find (id)) return;	// blacklisted
	
	if (threadPause != PAUSE_YES)
		{
		CoverCache_Pause (true);
		return;
		}

	DCFlushRange(cc, MAXITEMS * sizeof(s_cc));
	
	// Let's check if the item already exists...
	for (i = 0; i < MAXITEMS; i++)
		{
		if (*cc[i].id != '\0' && strcmp (id, cc[i].id) == 0) 
			{
			return;
			}
		}
	
	// first search blank...
	for (i = 0; i < MAXITEMS; i++)
		{
		if (*cc[i].id == '\0')
			{
			strcpy (cc[i].id, id);
			cc[i].age = age;
			cc[i].prio = priority;
			found = true;
			break;
			}
		}
		
	// If we haven't found the item search for the oldest one
	if (!found)
		{
		u32 minage = 0xFFFFFFFF;
		int minageidx = 0;
		
		for (i = 0; i < MAXITEMS; i++)
			{
			if (&cc[i].id != '\0' && cc[i].age < minage)
				{
				minage = cc[i].age;
				minageidx = i;
				}
			}
		
		int idx = minageidx;
		
		if (cc[idx].cover) 
			{
			FreeMem2Tex (cc[idx].cover);
			cc[idx].cover = NULL;
			}
			
		strcpy (cc[idx].id, id);
		cc[idx].age = age;
		cc[idx].prio = priority;
		}

	DCFlushRange(cc, MAXITEMS * sizeof(s_cc));
	age ++;
	}
