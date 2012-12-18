/*

forwarder dol used in postloader channel

load postloader from sd/isfs/fat32.

*/


#include <gccore.h>
#include <stdlib.h>
#include <ogc/machine/processor.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include "../build/bin2o.h"
#include "ios.h"
#include "debug.h"

#define VER "1.5"

#define EXECUTE_ADDR    ((u8 *) 0x92000000)
#define BOOTER_ADDR     ((u8 *) 0x93000000)
#define ARGS_ADDR       ((u8 *) 0x93200000)
#define CMDL_ADDR       ((u8 *) 0x93200000+sizeof(struct __argv))
#define HBMAGIC_ADDR    ((u8 *) 0x93200000-8)

#define TITLE_ID(x,y)	(((u64)(x) << 32) | (y))

#define NEEK 1

extern void __exception_closeall();
typedef void (*entrypoint) (void); 

u32 cookie;
void *exeBuffer = (void *) EXECUTE_ADDR;

#define CFG_HOME_WII_MENU   0x50756E65
#define MAXFILESIZE 16777216 // 16mb

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static int disableOutput = 0;

void InitVideo (void)
	{
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL); 
	xfb = SYS_AllocateFramebuffer(rmode);
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	xfb = MEM_K0_TO_K1(xfb);
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();
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
		ret = LoadFile("fat://apps/postloader/boot.dol");
		
		fatUnmount("fat:/");
		__io_wiisd.shutdown();
		}

	if (ret == false)
		ret = LoadPostloaderFromISFS ();

	if (ret == false && fatMountSimple("fat", &__io_usbstorage))
		{
		ret = LoadFile("fat://apps/postloader/boot.dol");
		
		fatUnmount("fat:/");
		__io_wiisd.shutdown();
		}
 
	return ret;
	} 

int main(int argc, char *argv[])
	{
	gprintf ("postLoader forwarder "VER"\n");
	
	InitVideo ();
	
	if (
		HBMAGIC_ADDR[0] == 'P' &&
		HBMAGIC_ADDR[1] == 'O' &&
		HBMAGIC_ADDR[2] == 'S' &&
		HBMAGIC_ADDR[3] == 'T'
	   )
		{
		disableOutput = 1;
		gprintf ("Magic world found !\n");
		goto directstart;
		}
	
	if (!disableOutput)
		{
		printf ("\n\n");
		printf ("Loading postLoader...");
		}
	
	gprintf ("Searching for postLoader\n");
	if (!GetFileToBoot ())
		{
		gprintf ("postLoader not found\n");
		printf ("\n\n\n");
		printf ("postloader not found... \n\n");
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
	
	memcpy(ARGS_ADDR, &arg, sizeof(arg));
	DCFlushRange(ARGS_ADDR, sizeof(arg) + arg.length);
	
directstart:

	HBMAGIC_ADDR[0] = 'X';
	HBMAGIC_ADDR[1] = 'X';
	HBMAGIC_ADDR[2] = 'X';
	HBMAGIC_ADDR[3] = 'X';
	
	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	entrypoint exeEntryPoint;
	exeEntryPoint = (entrypoint) BOOTER_ADDR;
	
	gprintf ("booting...\n");
	
	if  (!disableOutput)
		printf ("booting!");
	
	/* cleaning up and load dol */
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable (cookie);
	__exception_closeall ();
	exeEntryPoint ();
	_CPU_ISR_Restore (cookie);
	
	exit (0);
	}