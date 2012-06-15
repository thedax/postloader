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

#define MAXDBGBUFF 4096
char gprintfbuff[MAXDBGBUFF];

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
	started = 2;
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
	if (started == 2) return;
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

static char ascii(char s)
{
    if (s < 0x20) return '.';
    if (s > 0x7E) return '.';
    return s;
}

void gprintf (const char *format, ...)
{
	static int init = 0;
	
	if (!init)
		{
		init = 1;
		*gprintfbuff = '\0';
		}
	
	char * tmp = NULL;
	va_list va;
	va_start(va, format);
	
	if ((vasprintf(&tmp, format, va) >= 0) && tmp)
		{
		if ((strlen (gprintfbuff) + strlen(tmp)) < MAXDBGBUFF)
			strcat (gprintfbuff, tmp);
			
        usb_sendbuffer(EXI_CHANNEL_1, tmp, strlen(tmp));
		}
	va_end(va);

	if(tmp)
        free(tmp);
}

void gprintf_StoreBuff(char *fn)
	{
	FILE *f;
	
	f = fopen (fn, "ab");
	if (f)
		{
		fwrite (gprintfbuff, strlen(gprintfbuff), 1, f);
		fclose (f);
		
		*gprintfbuff = '\0';
		}
	}

void Debug_hexdump (void *d, int len)
{
    u8 *data;
    int i, off;
    data = (u8*) d;

    gprintf("\n       0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  0123456789ABCDEF");
    gprintf("\n====  ===============================================  ================\n");

    for (off = 0; off < len; off += 16)
    {
        gprintf("%04x  ", off);
        for (i = 0; i < 16; i++)
            if ((i + off) >= len)
                gprintf("   ");
            else gprintf("%02x ", data[off + i]);

        gprintf(" ");
        for (i = 0; i < 16; i++)
            if ((i + off) >= len)
                gprintf(" ");
            else gprintf("%c", ascii(data[off + i]));
        gprintf("\n");
    }
} 