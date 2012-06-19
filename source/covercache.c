#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#include "mem2.h"
#include "globals.h"

//static mutex_t mutex;

#define SET(a, b) a = b; DCFlushRange(&a, sizeof(a));
#define MAXITEMS 64

#define STACKSIZE	8192
static u8 * threadStack = NULL;
static lwp_t hthread = LWP_THREAD_NULL;

typedef struct 
	{
	char id[256];
	GRRLIB_texImg *cover;
	u8 prio;	// 1 or 0, 
	u32 age;
	}
s_cc; // covercache

/*
typedef struct 
	{
	char id[256];
	u8 prio;
	}
s_fifo; // covercache
*/

static s_cc *cc;
//static s_fifo *fifo;

static int idx = 0;	// used to add elements
static int running = 0;
static int update = 0;
static int age = 0;

static bool pauseThread = 0;
static bool stopThread = 0;

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

void CoverCache_Add (char *id, bool priority) // gameid without .png extension, if pt is true, thread will be paused
	{
	int i;
	bool found = false;
	
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
		
		idx = minageidx;
		
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

static void *thread (void *arg)
	{
	GRRLIB_texImg *tex;

	SET (running, 1);

	Debug ("covercache thread started");
	
	int i = 0;
	int doPrio = 0;

	while (true)
		{
		for (i = 0; i < MAXITEMS; i++)
			{
			if (*cc[i].id != '\0' && !cc[i].cover && ((doPrio == 1 && cc[i].prio == 1) || doPrio == 0))
				{
				tex = GRRLIB_LoadTextureFromFile (cc[i].id);
				
				cc[i].cover = MoveTex2Mem2 (tex);
				cc[i].prio = 0;
				
				if (!cc[i].cover) *cc[i].id = '\0'; // do not try again
				
				DCFlushRange(&cc[i], sizeof(s_cc));
				SET (update, update+1);
				}
				
			if (pauseThread)
				{
				LWP_SuspendThread(hthread);
				doPrio = 1;
				i = 0;
				}
			
			if (stopThread)
				{
				SET (running, 0);
				return NULL;
				}

			usleep (500);
			}
		
		doPrio = 0;
		/*
		for (i = 0; i < MAXITEMS; i++)
			{
			if (*fifo[i].id != '\0')
				{
				CoverCache_Feed (fifo[i].id, fifo[i].prio);
				*fifo[i].id = '\0';
				fifo[i].prio = 0;
				}
			}
		*/	
		// gprintf (".");
		}
	
	return NULL;
	}

/*
void CoverCache_Add (char *id, bool priority) // gameid without .png extension, if pt is true, thread will be paused
	{
	int i;
	DCFlushRange(fifo, MAXITEMS * sizeof(s_fifo));
	
	for (i = 0; i < MAXITEMS; i++)
		{
		if (*fifo[i].id == '\0')
			{
			strcpy (fifo[i].id, id);
			fifo[i].prio = priority;
			}
		}
	
	DCFlushRange(fifo, MAXITEMS * sizeof(s_fifo));
	}
*/
void CoverCache_Pause (bool yes) // return after putting thread in 
	{
	if (!running) return;
	
	if (yes)
		{
		SET (pauseThread, true);
		
		// wait for thread to finish
		while (!LWP_ThreadIsSuspended(hthread))
			{
			usleep(100);
			}
		}
	else
		{
		SET (pauseThread, false);
		
		LWP_ResumeThread(hthread); 
		}
	}
	
void CoverCache_Start (void)
	{
	Debug ("CoverCache_Start");

	cc = calloc (1, MAXITEMS * sizeof(s_cc));
	//memset (cc, 0, MAXITEMS * sizeof(s_cc));
	
	//fifo = mem2_malloc (MAXITEMS * sizeof(s_fifo));
	//memset (fifo, 0, MAXITEMS * sizeof(s_cc));
	
	threadStack = (u8 *) memalign(32, STACKSIZE);
	LWP_CreateThread (&hthread, thread, NULL, threadStack, STACKSIZE, 32);
	}

void CoverCache_Flush (void)	// empty the cache
	{
	int i;
	int count = 0;
	Debug ("CoverCache_Flush");
	
	CoverCache_Pause (true);
	
	DCFlushRange(cc, MAXITEMS * sizeof(s_cc));
	
	for (i = 0; i < MAXITEMS; i++)
		{
		if (cc[i].cover) 
			{
			//Debug ("CoverCache_Flush: 0x%08X %s", cc[i].cover, cc[i].id);
			FreeMem2Tex (cc[i].cover);
			//Debug ("CoverCache_Flush: (done)");
			cc[i].cover = NULL;
			
			count ++;
			}
			
		*cc[i].id = '\0';
		cc[i].age = 0;
		}
	age = 0;
	
	CoverCache_Pause (false);

	Debug ("CoverCache_Flush: %d covers flushed", count);
	}
	
void CoverCache_Stop (void)
	{
	Debug ("CoverCache_Stop");
	if (running)
		{
		SET (stopThread, 1);
		
		int i;
		for (i = 0; i < 2000; i++)
			{
			if (running == 0) break;
			usleep (1000);
			}
		
		if (running)
			{
			SET (stopThread, 1);
			Debug ("CoverCache_Stop: Warning, thread doesn't respond !");
			}
		
		//Debug ("CoverCache_Stop #1");
		LWP_JoinThread (hthread, NULL);

		//Debug ("CoverCache_Stop #2");
		CoverCache_Flush ();

		//Debug ("CoverCache_Stop #3");
		free (threadStack);

		//Debug ("CoverCache_Stop #5");
		free (cc);
		//mem2_free (fifo);

		Debug ("CoverCache_Stop completed");
		}
	else
		Debug ("CoverCache_Stop: thread was already stopped");
	}
	
GRRLIB_texImg *CoverCache_Get (char *id) // this will return the same text
	{
	if (running == 0) return NULL;

	int i;
	GRRLIB_texImg *tex = NULL;

	//CoverCache_Pause (true);
	
	for (i = 0; i < MAXITEMS; i++)
		{
		if (*cc[i].id != '\0' && strcmp (id, cc[i].id) == 0)
			{
			tex = cc[i].cover;
			break;
			}
		}
		
	//CoverCache_Pause (false);

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
