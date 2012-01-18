#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ogc/usbgecko.h>
#include <ogc/exi.h>

/*

Debug, will write debug information to sd and/or gecko.... as debug file is open/closed it will be VERY SLOW

*/

#define DEBUG_MAXCACHE 32
static char dbgfile[64];
static char *cache[DEBUG_MAXCACHE];

static int started = 0;
static int filelog = 0;
static int geckolog = 0;

s32 DebugStart (bool gecko, char *fn)
	{
	filelog = 0;
	started = 0;
	
	sprintf (dbgfile, fn);

	geckolog = usb_isgeckoalive (EXI_CHANNEL_1);
	
	// check if the file exist
	FILE * f = NULL;
	f = fopen(dbgfile, "rb");
	if (f) 
		{
		filelog = 1;
		fclose (f);
		}
	
	memset (cache, 0, sizeof(cache));
	
	if (filelog || geckolog)
		started = 1;
	
	return started;
	}
	
void DebugStop (void)
	{
	filelog = 0;
	started = 0;
	}

void Debug (const char *text, ...)
	{
	if (!started || text == NULL) return;
		
	int i;
	char mex[1024];
	FILE * f = NULL;

	va_list argp;
	va_start (argp, text);
	vsprintf (mex, text, argp);
	va_end (argp);
	
	strcat (mex, "\r\n");

	if (geckolog)
		{
		usb_sendbuffer( EXI_CHANNEL_1, mex, strlen(mex) );
		usb_flush(EXI_CHANNEL_1);
		}
	
	if (filelog == 0) return;
	
	// If a message start with '@', do not open... it will be cached cache it...
	if (mex[0] != '@')
		f = fopen(dbgfile, "ab");

	//if file cannot be opened, cannot open the file, maybe filesystem unmounted or nand emu active... use cache
	if (f)
		{
		for (i = 0; i < DEBUG_MAXCACHE; i++)
			{
			if (cache[i] != NULL)
				{
				fwrite (cache[i], 1, strlen(mex), f );
				free (cache[i]);
				cache[i] = NULL;
				}
			}
		fwrite (mex, 1, strlen(mex), f );
		fclose(f);
		}
	else
		{
		for (i = 0; i < DEBUG_MAXCACHE; i++)
			{
			if (cache[i] == NULL)
				{
				cache[i] = calloc (strlen(mex) + 1, 1);
				strcpy (cache[i], mex);
				break;
				}
			}
		}
	}
