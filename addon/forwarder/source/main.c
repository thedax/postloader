/*

priiBooter is a small programm to be added to priiloader. It allow to spawn anoteher program

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
#include "hbcstub.h"
#include "ios.h"
#include "debug.h"

#define VER "1.3"

#define EXECUTE_ADDR    ((u8 *) 0x92000000)
#define BOOTER_ADDR     ((u8 *) 0x93000000)
#define ARGS_ADDR       ((u8 *) 0x93200000)
#define CMDL_ADDR       ((u8 *) 0x93200000+sizeof(struct __argv))
#define HBMAGIC_ADDR    ((u8 *) 0x93200000-8)

#define TITLE_ID(x,y)		(((u64)(x) << 32) | (y))

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
	VIDEO_Init();
	VIDEO_SetBlack(TRUE); 
	
	rmode = VIDEO_GetPreferredMode(NULL);

	//apparently the video likes to be bigger then it actually is on NTSC/PAL60/480p. lets fix that!
	if( rmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
	{
		//the correct one would be * 0.035 to be sure to get on the Action safe of the screen.
		GX_AdjustForOverscan(rmode, rmode, 0, rmode->viWidth * 0.026 ); 
	}
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_ClearFrameBuffer (rmode, xfb, 0); 
	
	console_init( xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth*VI_DISPLAY_PIX_SZ );

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	
	//gprintf("resolution is %dx%d\n",rmode->viWidth,rmode->viHeight);
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

	if (ret == false)
		ret = LoadPostloaderFromISFS ();

	if (ret == false && fatMountSimple("fat", &__io_usbstorage))
		{
		if (ret == false) ret = LoadFile("fat://apps/postloader/boot.dol");
		if (ret == false) ret = LoadFile("fat://postloader.dol");
		if (ret == false) ret = LoadFile("fat://boot.dol");
		if (ret == false) ret = LoadFile("fat://boot.elf");
		
		fatUnmount("fat:/");
		__io_wiisd.shutdown();
		}
 
	return ret;
	} 

int main(int argc, char *argv[])
	{
	gprintf ("postLoader forwarder "VER"\n");
	
	InitVideo ();
	
	StubLoad ();

	if (
		HBMAGIC_ADDR[0] == 'P' &&
		HBMAGIC_ADDR[1] == 'O' &&
		HBMAGIC_ADDR[2] == 'S' &&
		HBMAGIC_ADDR[3] == 'T'
	   )
		{
		gprintf ("Magic world found !\n");
		goto directstart;
		}
	
	gprintf ("Searching for postLoader\n");
	if (!GetFileToBoot ())
		{
		gprintf ("postLoader not found\n");
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
	
	memcpy(ARGS_ADDR, &arg, sizeof(arg));
	DCFlushRange(ARGS_ADDR, sizeof(arg) + arg.length);
	
directstart:

	HBMAGIC_ADDR[0] = 'X';
	HBMAGIC_ADDR[1] = 'X';
	HBMAGIC_ADDR[2] = 'X';
	HBMAGIC_ADDR[3] = 'X';
	
	Set_Stub (TITLE_ID(0x00010001,0x504f5354));

	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	entrypoint exeEntryPoint;
	exeEntryPoint = (entrypoint) BOOTER_ADDR;
	
	gprintf ("booting...\n");
	
	/* cleaning up and load dol */
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable (cookie);
	__exception_closeall ();
	exeEntryPoint ();
	_CPU_ISR_Restore (cookie);
	
	exit (0);
	}