#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <gccore.h>
#include <unistd.h>
#include <wiiuse/wpad.h>
#include <dirent.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void green_fix(void) //GREENSCREEN FIX
	{
	if (xfb == NULL)
		{
		rmode = VIDEO_GetPreferredMode(NULL);
		xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
		}
	
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(TRUE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
	}

void InitVideo (void)
	{
	// Initialise the video system
	VIDEO_Init();
	
	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	int x, y, w, h;
	x = 20;
	y = 32;
	w = rmode->fbWidth - (32);
	h = rmode->xfbHeight - (48);
	
	CON_InitEx(rmode, x, y, w, h);
	
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	CON_InitEx(rmode, x, y, w, h);
	
	// Set console text color
	printf("\x1b[%u;%um", 37, false);
	printf("\x1b[%u;%um", 40, false);
	}
	
void printd(const char *text, ...)
	{
	return;
	
	char Buffer[1024];
	va_list args;

	va_start(args, text);
	vsprintf(Buffer, text, args);

	va_end(args);
	
	printf (Buffer);
	printf ("\n");
	
	if (strstr (Buffer, "ERR:"))
		sleep (3);
	else
		usleep (50*1000);
	}
