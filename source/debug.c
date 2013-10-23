#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ogc/usbgecko.h>
#include <ogc/exi.h>

/*

Debug, will write debug information to sd and/or gecko.... as debug file is open/closed it will be VERY SLOW

*/

//#define DEBUGDISABLED

static char dbgfile[64];
static int started = 0;
static int geckolog = 0;
static FILE * fdebug = NULL;
s32 DebugStart (bool gecko, char *fn)
	{
#ifdef DEBUGDISABLED
	return;
#else
	started = 0;
	
	sprintf (dbgfile, fn);

	geckolog = usb_isgeckoalive (EXI_CHANNEL_1);
	
	// Open debug file
	fdebug = fopen(dbgfile, "rb");
	if (fdebug)
		{
		fclose (fdebug);
		fdebug = fopen(dbgfile, "ab");
		}
	
	if (fdebug || geckolog)
		started = 1;
	
	return started;
#endif	
	}
	
void DebugStop (void)
	{
#ifdef DEBUGDISABLED
	return;
#endif
	if (fdebug)
		fclose (fdebug);

	fdebug = NULL;
	started = 2;
	}

void Debug (const char *text, ...)
	{
#ifdef DEBUGDISABLED
	return;
#else	
	if (!started || text == NULL) return;
		
	char mex[2048];

	va_list argp;
	va_start (argp, text);
	vsprintf (mex, text, argp);
	va_end (argp);
	
	strcat (mex, "\r\n");

	if (geckolog)
		{
		usb_sendbuffer( EXI_CHANNEL_1, mex, strlen(mex) );
		usb_flush(EXI_CHANNEL_1);
		usleep (500);
		}

	if (started == 2) return;
	
	int ret = -1;
	if (fdebug)
		{
		ret = fwrite (mex, 1, strlen(mex), fdebug );
		fflush (fdebug);
		}
#endif		
	}

static char ascii(char s)
{
    if (s < 0x20) return '.';
    if (s > 0x7E) return '.';
    return s;
}

void gprintf (const char *format, ...)
{
#ifdef DEBUGDISABLED
	return;
#else
	char * tmp = NULL;
	va_list va;
	va_start(va, format);
	
	if((vasprintf(&tmp, format, va) >= 0) && tmp)
	{
        usb_sendbuffer(EXI_CHANNEL_1, tmp, strlen(tmp));
	}
	va_end(va);

	if(tmp)
        free(tmp);
#endif
}

void Debug_hexdump (void *d, int len)
{
#ifdef DEBUGDISABLED
	return;
#else
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
#endif	
} 
void Debug_hexdumplog (void *d, int len)
{
#ifdef DEBUGDISABLED
	return;
#else
    u8 *data;
    int i, off;
	char b[2048] = {0};
	char bb[256];
		
    data = (u8*) d;

    sprintf(bb, "\n       0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  0123456789ABCDEF");
	strcat(b, bb);
    sprintf(bb, "\n====  ===============================================  ================\n");
	strcat(b, bb);

    for (off = 0; off < len; off += 16)
    {
        sprintf(bb, "%04x  ", off);
		strcat(b, bb);
        for (i = 0; i < 16; i++)
            if ((i + off) >= len)
				{
                sprintf(bb, "   ");
				strcat(b, bb);
				}
            else 
				{
				sprintf(bb, "%02x ", data[off + i]);
				strcat(b, bb);
				}

        sprintf(bb, " ");
		strcat(b, bb);
		
        for (i = 0; i < 16; i++)
            if ((i + off) >= len)
				{
                sprintf(bb, " ");
				strcat(b, bb);
				}
            else 
				{
				sprintf(bb, "%c", ascii(data[off + i]));
				strcat(b, bb);
				}
				
        sprintf(bb, "\n");
		strcat(b, bb);
    }
	Debug (b);
#endif	
} 