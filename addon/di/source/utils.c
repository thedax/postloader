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
	
	printf(Buffer);
	
	if (strstr (Buffer, "ERR:"))
		sleep (3);
	else
		usleep (1000 * 100);
	}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
u8 *ReadFile2Buffer (char *path, size_t *filesize, int *err)
	{
	u8 *buff = NULL;
	int size;
	int bytes;
	int block = 65536;
	FILE* f = NULL;
	
	if (filesize) *filesize = 0;
	if (err) *err = 0;
	
	f = fopen(path, "rb");
	if (!f)
		{
		if (err != NULL) *err = -1;
		return NULL;
		}

	//Get file size
	fseek( f, 0, SEEK_END);
	size = ftell(f);
	if (filesize) *filesize = size;
	
	if (size <= 0)
		{
		if (err != NULL) *err = -2;
		fclose (f);
		return NULL;
		}
		
	// Return to beginning....
	fseek( f, 0, SEEK_SET);
	
	buff = malloc (size);
	if (buff == NULL) 
		{
		if (err != NULL) *err = -3;
		fclose (f);
		return NULL;
		}
	
	bytes = 0;
	do
		{
		bytes += fread(&buff[bytes], 1, block, f );
		}
	while (bytes < size);

	fclose (f);
	
	return buff;
	}
	
bool FileExist (char *fn)
	{
	FILE * f;
	f = fopen(fn, "rb");
	if (!f) return FALSE;
	fclose(f);
	return TRUE;
	}
	
bool DirExist (char *path)
	{
	DIR *dir;
	
	dir=opendir(path);
	if (dir)
		{
		closedir(dir);
		return TRUE;
		}
	
	return FALSE;
	}
