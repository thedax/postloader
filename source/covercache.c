#include <gccore.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include "mp3player.h"

#include "mem2.h"
#include "globals.h"
#include "mystring.h"

#define MAXITEMS 192
#define PRIO 32
#define TSIZE 192.0
#define ELEMENT ((int)TSIZE * (int)TSIZE * 4)
#define BLOCK (ELEMENT * MAXITEMS)

static mutex_t mutex;

#define SET(a, b) a = b;
//#define SET(a, b) a = b; DCFlushRange(&a, sizeof(a)); ICInvalidateRange(&a, sizeof(a));
//#define FRESH(a) DCFlushRange(&a, sizeof(a)); ICInvalidateRange(&a, sizeof(a));
//#define MAXITEMS 128

#define STACKSIZE	8192
static u8 * cache = NULL;
static u8 * threadStack = NULL;
static lwp_t hthread = LWP_THREAD_NULL;
//static lwp_t htmon = LWP_THREAD_NULL;

typedef struct 
	{
	char id[256];
	GRRLIB_texImg *cover;
	u8 prio;	// 1 or 0, 
	u32 age;
	}
s_cc; // covercache

static s_cc *cc = NULL;

static int age = 0;
static volatile bool threadPaused = false;
static volatile bool threadPause = false;
static volatile bool threadStop = false;
static volatile bool threadRunning = false;
//static volatile bool monrunning = false;
static volatile int cycle = 0;
static volatile int cId = 0;
static volatile int update = 0;

#define UPDATE_TP() DCFlushRange(threadPause, sizeof(int))

GRRLIB_texImg*  CoverCache_LoadTextureFromFile(char *filename);

/*
static void *tmon (void *arg)
	{
	monrunning = true;
	while (monrunning)
		{
		gprintf ("tmon: %d %d %d %d %d\n", threadPaused, threadPause, threadStop, threadRunning, cycle);
		usleep (200000);
		}
		
	return NULL;
	}
*/

static GRRLIB_texImg *MoveTex2Mem2 (GRRLIB_texImg *tex, int index)
	{
	if (!tex || !tex->data) return NULL;
	
	u32 size = tex->w * tex->h * 4;
	
	if (size == 0 || size > ELEMENT)
		{
		GRRLIB_FreeTexture (tex);
		return NULL;
		}
		
	u8* data = &cache[index * ELEMENT];
	memcpy (data, tex->data, size);

	free (tex->data);
	tex->data = data;
	
	GRRLIB_FlushTex (tex);
	
	return tex;
	}
	
static void FreeMem2Tex (GRRLIB_texImg *tex)
	{
	DCFlushRange(tex, sizeof(GRRLIB_texImg));
	if (!tex) return;
	free(tex);
	}

static void *thread (void *arg)
	{
	GRRLIB_texImg *tex;
	char name[256];

	threadRunning = 1;

	int doPrio = 0;
	int i;
	int read;
	
	while (true)
		{
		read = 0;
		
		cycle++;
		
		for (i = 0; i < MAXITEMS; i++)
			{
			if (*cc[i].id != '\0' && !cc[i].cover && ((doPrio == 1 && cc[i].prio == 1) || doPrio == 0))
				{
				LWP_MutexLock (mutex);
				read = 1;
				cId = i;
				strcpy (name, cc[i].id);
				LWP_MutexUnlock (mutex);
				
				MP3Player_Lock (true);
				tex = CoverCache_LoadTextureFromFile (name);
				MP3Player_Lock (false);

				LWP_MutexLock (mutex);
				cc[i].prio = 0;
 				cc[i].cover = MoveTex2Mem2 (tex, i);
				
				if (!cc[i].cover) 
					{
					*cc[i].id = '\0'; // do not try again
					}
				DCFlushRange(&cc[i], sizeof(s_cc));
				update ++;
				
				LWP_MutexUnlock (mutex);

				cId = -1;
				
				usleep (10);
				}
				
			if (threadPause)
				{
				LWP_SuspendThread(hthread);
				doPrio = 1;
				i = 0;
				}
			
			if (threadStop)
				{
				threadRunning = 0;
				return NULL;
				}
			}
			
		if (read == 0) 
			{
			usleep (1000);
			}

		doPrio = 0;
		}
	
	return NULL;
	}

void CoverCache_Pause (bool yes) // return after putting thread in 
	{
	if (!threadRunning) return;
	
	if (yes)
		{
		SET (threadPause,true);
		
		get_msec(true);
		while (!LWP_ThreadIsSuspended(hthread)) 
			{
			usleep (1000);
			if (get_msec(false) > 1000)
				{
				Debug ("CoverCache_Pause pause: timeout, retring...");
				break;
				}
			}
		}
	else
		{
		SET (threadPause,false);
		LWP_ResumeThread (hthread);

		get_msec(true);
		while (LWP_ThreadIsSuspended(hthread)) 
			{
			usleep (1000);
			if (get_msec(false) > 1000)
				{
				Debug ("CoverCache_Pause continue: timeout, retring...");
				break;
				}
			}
		}
	}
	
void CoverCache_Start (void)
	{
	Debug ("CoverCache_Start");
	
	cc = calloc (1, MAXITEMS * sizeof(s_cc));
	cache = mem2_malloc (BLOCK);
	
	Debug ("cache = 0x%X (size = %u Kb)", cache, BLOCK);

	mutex = LWP_MUTEX_NULL;
    LWP_MutexInit (&mutex, false);

	LWP_CreateThread (&hthread, thread, NULL, NULL, 0, PRIO);
	LWP_ResumeThread(hthread);

	//LWP_CreateThread (&htmon, tmon, NULL, NULL, 0, PRIO);
	//LWP_ResumeThread(htmon);
	}

void CoverCache_Stop (void)
	{
	if (!cc) return; // start wasn't called
	
	Debug ("CoverCache_Stop");
	//monrunning = false;
	if (threadRunning)
		{
		SET (threadStop,1);
		LWP_JoinThread (hthread, NULL);
		CoverCache_Flush ();
		free (threadStack);
		free (cc);
		mem2_free (cache);
		LWP_MutexDestroy (mutex);
		}
	else
		Debug ("CoverCache_Stop: thread was already stopped");
	}
	
void CoverCache_Flush (void)	// empty the cache
	{
	if (!cc) return; // start wasn't called
	
	int i;
	int count = 0;
	Debug ("CoverCache_Flush");
	
	CoverCache_Pause (true);
	
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
	
GRRLIB_texImg *CoverCache_Get (char *id) // this will return the same text
	{
	if (threadRunning == 0) return NULL;

	int i;
	GRRLIB_texImg *tex = NULL;
	
	LWP_MutexLock (mutex);
	
	for (i = 0; i < MAXITEMS; i++)
		{
		if (i != cId && *cc[i].id != '\0' && strcmp (id, cc[i].id) == 0)
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

void CoverCache_Add (char *id, bool priority)
	{
	int i;
	bool found = false;
	
	if (!threadPause)
		CoverCache_Pause (true);

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

/*

*/
u8 * LoadRGBTexRGBA (char *fn, u16* w, u16* h, u8 alpha)
	{
	u16 *wh;

	u32 x, y;

	size_t size_rgb;
	u8* rgb;
	u8* prgb;

	u32 size_rgba;
	u8* rgba;
	
	*w = 0;
	*h = 0;
	
	//Debug ("Loading %s", fn);
	
	rgb = fsop_ReadFile (fn, 0, &size_rgb);
	if (!rgb)
		{
		//Debug ("...not found", fn);
		return NULL;
		}
	
	wh = (u16*)(rgb + size_rgb - 4);
	
	//Debug ("size = %d, %d, %d, %u", size_rgb, wh[0], wh[1], (void*)wh-(void*)rgb);
	
	if (wh[0] == 0 || wh[1] == 0 || wh[0] > TSIZE || wh[1] > TSIZE)
		{
		free (rgb);
		return NULL;
		}
	
	size_rgb = wh[0]*wh[1]*3;
	size_rgba = wh[0]*wh[1]*4;
		
	rgba = malloc (size_rgba);
	if (!rgba)
		{
		free (rgb);
		return NULL;
		}
	
	prgb = rgb;
	for (x = 0; x < wh[0]; x++)
		for (y = 0; y < wh[1]; y++)
			{
			GRRLIB_SetPixelTotexImg4x4RGBA8 (x, y, wh[0], rgba, RGBA (prgb[0], prgb[1], prgb[2], alpha));
			prgb += 3;
			}
	
		
	free (rgb);
	
	*w = wh[0];
	*h = wh[1];
	return rgba; 
	}

bool SaveRGBATexRGB (char *fn, u8* rgba, u16 w, u16 h)
	{
	FILE *f;
	u16 wh[2];
	u32 x,y;
	u32 size_rgb;
	u8* rgb;
	u8* prgb;
	u32 pixel;
	
	size_rgb = w*h*3;

	rgb = malloc (size_rgb);
	if (!rgb)
		{
		return false;
		}

	prgb = rgb;
	for (x = 0; x < w; x++)
		for (y = 0; y < h; y++)
			{
			pixel = GRRLIB_GetPixelFromtexImg4x4RGBA8 (x, y, w, rgba);
			*(prgb++) = R(pixel);
			*(prgb++) = G(pixel);
			*(prgb++) = B(pixel);
			}
	
	f = fopen (fn, "wb");
	if (!f) 
		{
		//Debug ("SaveRGBATexRGB: cannot write to '%s'", fn);
		free (rgb);
		return false;
		}
	
	fwrite (rgb, 1, size_rgb, f);
	wh[0] = w;
	wh[1] = h;
	fwrite (wh, 1, sizeof(wh), f);

	fclose (f);
		
	free (rgb);
	return true; 
	}

u8 * LoadRGBATex (char *fn, u16* w, u16* h, u8 alpha)
	{
	u32 size_rgba;
	u8* rgba;
	u16 *wh;
	
	*w = 0;
	*h = 0;
	
	rgba = fsop_ReadFile (fn, 0, &size_rgba);
	if (!rgba)
		{
		Debug ("'%s' NOT loaded ...", fn);
		return NULL;
		}
	
	wh = (u16*)(rgba + size_rgba - 4);

	*w = wh[0];
	*h = wh[1];
	
	Debug ("'%s' loaded succesfully...", fn);

	return rgba; 
	}

bool SaveRGBATex (char *fn, u8* rgba, u16 w, u16 h)
	{
	FILE *f;
	u16 wh[2];
	u32 size_rgba;
	
	if (w == 0 || h == 0 || !rgba) return false;
	
	size_rgba = w*h*4;

	f = fopen (fn, "wb");
	if (!f) 
		{
		//Debug ("SaveRGBATexRGB: cannot write to '%s'", fn);
		return false;
		}
	
	fwrite (rgba, 1, size_rgba, f);
	wh[0] = w;
	wh[1] = h;
	fwrite (wh, 1, sizeof(wh), f);

	fclose (f);
	return true; 
	}

GRRLIB_texImg*  CoverCache_LoadTextureFromFile(char *filename) 
	{
    GRRLIB_texImg  *tex = NULL;
    unsigned char  *data;
	char fntex[256];
	char buff[256];
	u8 *rgba;
	u16 i, j, w, h;
	float fw, fh;
	
	Debug ("CoverCache_LoadTextureFromFile '%s'", filename);
	
	// let's clean the filename
	strcpy (fntex, filename);

	// kill extension
	j = strlen (fntex);
	while (j > 0 && fntex[j--] != '.');
	fntex[j+1] = 0;
	
	j = 0;
	for (i = 0; i < strlen(fntex); i++)
		{
		if ((fntex[i] >= 48 && fntex[i] <= 57) || (fntex[i] >= 65 && fntex[i] <= 90) || (fntex[i] >= 97 && fntex[i] <= 122))
			{
			buff[j++] = fntex[i];
			}
		}
	buff[j] = 0;
	ms_strtolower(buff);
	
	// let's take max 16 char for texname
	if (j < 16)
		{
		sprintf (fntex, "%s://ploader/tex/%s.tex", vars.defMount, buff);
		}
	else
		{
		sprintf (fntex, "%s://ploader/tex/%s.tex", vars.defMount, &buff[j-16]);
		}
	
	//Debug ("fntex = %s", fntex);
		
	rgba = LoadRGBATex (fntex, &w, &h, 255);
	if (!rgba)
		{
		// return NULL it load fails
		if (GRRLIB_LoadFile(filename, &data) <= 0)  goto end;
		// Convert to texture
		tex = GRRLIB_LoadTexture(data);
		// Free up the buffer
		free(data);
		
		if (!tex) return NULL;
		
		if (tex->w >= tex->h && tex->w > TSIZE)
			{
			fw = TSIZE;
			fh = TSIZE / ((float)tex->w / (float)tex->h);;
			}
		else if (tex->w < tex->h && tex->h > TSIZE)
			{
			fw = TSIZE / ((float)tex->h / (float)tex->w);
			fh = TSIZE;
			}
		else
			{
			fw = tex->w;
			fh = tex->h;
			}
			
		w = (u16)fw / 4 * 4;
		h = (u16)fh / 4 * 4;
			
		rgba = malloc (w * h *4);
		ResizeRGBA (tex->data, tex->w, tex->h, rgba, w, h);
		GRRLIB_FreeTexture (tex);

		if (config.enableTexCache)
			SaveRGBATex (fntex, rgba, w, h);
		}

	tex = calloc(1, sizeof(GRRLIB_texImg));
	tex->w = w;
	tex->h = h;
	tex->data = rgba;
	GRRLIB_SetHandle( tex, 0, 0 );
	GRRLIB_FlushTex( tex );
	
	end:
	
	Debug ("CoverCache_LoadTextureFromFile 0x%X", tex);

    return tex;
	}
