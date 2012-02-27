#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#include "mem2.h"
#include "globals.h"

mutex_t mutex;

#define MAXITEMS 128

#define STACKSIZE	8192

static u8 * threadStack = NULL;
static lwp_t hthread = LWP_THREAD_NULL;

typedef struct 
	{
	char id[128];
	GRRLIB_texImg *cover;
	u32 age;
	}
s_cc; // covercache

static s_cc *cc;

static int idx = 0;	// used to add elements
static int running = 0;
static int update = 0;
static int age = 0;

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
	
	if (tex->data)
		mem2_free(tex->data);
		
	mem2_free(tex);
	}

static void *thread (void *arg)
	{
	GRRLIB_texImg *tex;

	running = 1;

	Debug ("covercache thread started");
	int i = 0;

	while (running)
		{
		usleep (1000); // 10 msec
		
		for (i = 0; i < MAXITEMS; i++)
			{
			if (*cc[i].id != '\0' && !cc[i].cover)
				{
				tex = GRRLIB_LoadTextureFromFile (cc[i].id);

				LWP_MutexLock (mutex);
				cc[i].cover = MoveTex2Mem2 (tex);
				
				if (!cc[i].cover) *cc[i].id = '\0'; // do not try again
				
				update++;
				DCFlushRange(&update, sizeof(update));
				LWP_MutexUnlock (mutex);	
				
				}
			
			usleep (10);
			if (!running) break;
			}
		}
	
	running = -1;
	
	return NULL;
	}

void CoverCache_Pause (bool yes) // return after putting thread in 
	{
	if (running == 0) return;
	
	if (yes)
		{
		LWP_MutexLock (mutex);
		}
	else
		{
		LWP_MutexUnlock (mutex);
		}
	}
	
void CoverCache_Start (void)
	{
	Debug ("CoverCache_Start");
	cc = mem2_malloc (MAXITEMS * sizeof(s_cc));
	memset (cc, 0, MAXITEMS * sizeof(s_cc));

	mutex = LWP_MUTEX_NULL;
	LWP_MutexInit (&mutex, false);
	threadStack = (u8 *) memalign(32, STACKSIZE);
	LWP_CreateThread (&hthread, thread, NULL, threadStack, STACKSIZE, 30);
	LWP_ResumeThread(hthread);
	}

void CoverCache_Flush (void)	// empty the cache
	{
	int i;
	Debug ("CoverCache_Flush");
	
	LWP_MutexLock (mutex);
	
	for (i = 0; i < MAXITEMS; i++)
		{
		if (cc[i].cover) 
			{
			Debug ("CoverCache_Flush: %d '%s'", i, cc[i].id);
			FreeMem2Tex (cc[i].cover);
			cc[i].cover = NULL;
			}
			
		*cc[i].id = '\0';
		cc[i].age = 0;
		}
	age = 0;
	
	LWP_MutexUnlock (mutex);
	}
	
void CoverCache_Stop (void)
	{
	Debug ("CoverCache_Stop");
	if (running)
		{
		running = 0;
		while (running != -1)
			{
			usleep (100);
			}

		Debug ("CoverCache_Stop #1");
		LWP_JoinThread (hthread, NULL);
		
		Debug ("CoverCache_Stop #2");
		CoverCache_Flush ();

		Debug ("CoverCache_Stop #3");
		free (threadStack);

		Debug ("CoverCache_Stop #4");
		LWP_MutexDestroy (mutex);

		Debug ("CoverCache_Stop #5");
		mem2_free (cc);

		Debug ("CoverCache_Stop #6");
		}
	}
		
void CoverCache_Add (char *id, bool pt) // gameid without .png extension, if pt is true, thread will be paused
	{
	int i;
	bool found = false;
	
	if (pt) CoverCache_Pause (true);
	
	DCFlushRange(cc, MAXITEMS * sizeof(s_cc));
	
	// Let's check if the item already exists...
	for (i = 0; i < MAXITEMS; i++)
		{
		if (*cc[i].id != '\0' && strcmp (id, cc[i].id) == 0) 
			{
			if (pt) CoverCache_Pause (false);
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
		
		idx = minageidx;
		
		if (cc[idx].cover) 
			{
			FreeMem2Tex (cc[idx].cover);
			cc[idx].cover = NULL;
			}
			
		strcpy (cc[idx].id, id);
		cc[idx].age = age;
		}

	DCFlushRange(cc, MAXITEMS * sizeof(s_cc));
	age ++;
	
	if (pt) CoverCache_Pause (false);
	}
	
GRRLIB_texImg *CoverCache_Get (char *id) // this will return the same text
	{
	if (running == 0) return NULL;

	int i;
	GRRLIB_texImg *tex = NULL;

	LWP_MutexLock (mutex);
	for (i = 0; i < MAXITEMS; i++)
		{
		if (*cc[i].id != '\0' && strcmp (id, cc[i].id) == 0)
			{
			tex = cc[i].cover;
			break;
			}
		}
	LWP_MutexUnlock (mutex);

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
