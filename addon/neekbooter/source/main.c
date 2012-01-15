/*

priiBooter is a small programm to be added to priiloader. It allow to spawn anoteher program

*/


#include <gccore.h>
#include <ogc/machine/processor.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include "../build/bin2o.h"

#define VER "1.2"

#define EXECUTE_ADDR    ((u8 *) 0x92000000)
#define BOOTER_ADDR     ((u8 *) 0x93000000)
#define ARGS_ADDR       ((u8 *) 0x93200000)
#define CMDL_ADDR       ((u8 *) 0x93200000+sizeof(struct __argv))

#define NEEK 1

extern void __exception_closeall();
typedef void (*entrypoint) (void); 

u32 cookie;
void *exeBuffer = (void *) EXECUTE_ADDR;

#define CFG_HOME_WII_MENU   0x50756E65
#define MAXFILESIZE 16777216 // 16mb

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

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

bool LoadFile (char *path)
	{
	FILE *f;
	size_t size;
	
	f = fopen(path, "rb");
	if (!f) return false;
	
	size = fread(exeBuffer, 1, MAXFILESIZE, f );
	fclose (f);
	
	if (size <= 0) return false;

	return true;
	}

bool LoadPostloaderFromISFS (void)
	{
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	ISFS_Initialize();

	strcpy (path, "/apps/postLoader/boot.dol");

	s32 fd = ISFS_Open (path, ISFS_OPEN_READ);
	if (fd < 0) 
		goto fail;
	s32 filesize = ISFS_Seek(fd, 0, 2);
	if (filesize == 0)
		goto fail;
	ISFS_Seek (fd, 0, 0);

	// exeBuffer is already 32bit aligned... should work fine
	s32 readed = ISFS_Read (fd, exeBuffer, filesize);
	ISFS_Close (fd);
	
	if (readed != filesize)
		goto fail;
	
	return TRUE;
	
	fail:
	ISFS_Deinitialize ();
	return FALSE;
	}

bool GetFileToBoot (void)
	{
	bool ret = false;
	
	if (ret == false && fatMountSimple("fat", &__io_wiisd))
		{
		if (ret == false) ret = LoadFile("fat://apps/postloader/boot.dol");
		if (ret == false) ret = LoadFile("fat://postloader.dol");
		if (ret == false) ret = LoadFile("fat://boot.dol");
		if (ret == false) ret = LoadFile("fat://boot.elf");
		
		fatUnmount("fat:/");
		__io_wiisd.shutdown();
		}

	if (ret == false && fatMountSimple("fat", &__io_usbstorage))
		{
		if (ret == false) ret = LoadFile("fat://apps/postloader/boot.dol");
		if (ret == false) ret = LoadFile("fat://postloader.dol");
		if (ret == false) ret = LoadFile("fat://boot.dol");
		if (ret == false) ret = LoadFile("fat://boot.elf");
		
		fatUnmount("fat:/");
		__io_wiisd.shutdown();
		}

	if (ret == false)
		ret = LoadPostloaderFromISFS ();
 
	return ret;
	} 

int main(int argc, char *argv[])
	{
	if (!GetFileToBoot ())
		{
		InitVideo ();
		
		printf ("\n\n\n");
		printf ("neekbooter: postloader not found... \n\n");
		printf ("booting to system menu...");
		
		sleep (3);
		
		// Let's try to start system menu... use magic word, as we may have priiloader behind
		*(vu32*)0x8132FFFB = CFG_HOME_WII_MENU;
		DCFlushRange((void*)0x8132FFFB,4);
		
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
		return 0;
		}
		
	struct __argv arg;
	memset (&arg, 0, sizeof(struct __argv));
	
	// Passing neek to postloader is no more needed, anyway keep it, as can be usefull
	if (NEEK)
		{
		char buff[32];

		memset (buff, 0, sizeof(buff));
		strcpy (buff, "neek");

		arg.argvMagic = ARGV_MAGIC;
		arg.length  = strlen(buff)+1;
		arg.commandLine = (char*)CMDL_ADDR;
		strcpy(arg.commandLine, buff);
		}

	memmove(ARGS_ADDR, &arg, sizeof(arg));
	DCFlushRange(ARGS_ADDR, sizeof(arg) + arg.length);

	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	entrypoint exeEntryPoint;
	exeEntryPoint = (entrypoint) BOOTER_ADDR;
	
	/* cleaning up and load dol */
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable (cookie);
	__exception_closeall ();
	exeEntryPoint ();
	_CPU_ISR_Restore (cookie);
	return 0;
	}