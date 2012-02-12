#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <gccore.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <ogc/machine/processor.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include "neek.h"
#include "bin2o.h"

#define VER "[1.2]"

#define EXECUTE_ADDR    ((u8 *) 0x92000000)
#define BOOTER_ADDR     ((u8 *) 0x93000000)
#define ARGS_ADDR       ((u8 *) 0x93200000) 
#define CMDL_ADDR       ((u8 *) 0x93200000+sizeof(struct __argv))

#define TITLE_ID(x,y)		(((u64)(x) << 32) | (y))

#define MAXFILESIZE 16777216 // 16mb

#define Debug printd

u32 cookie;
void *exeBuffer = (void *) EXECUTE_ADDR;

// This must reflect postloader ---------------------------------------------------------------
typedef struct
	{
	int priority;	// Vote !
	bool hidden;	// if 1, this app will be not listed
	
	u64 titleId; 	// title id
	u8 ios;		 	// ios to reload
	u8 vmode;	 	// 0 Default Video Mode	1 NTSC480i	2 NTSC480p	3 PAL480i	4 PAL480p	5 PAL576i	6 MPAL480i	7 MPAL480p
	s8 language; 	//	-1 Default Language	0 Japanese	1 English	2 German	3 French	4 Spanish	5 Italian	6 Dutch	7 S. Chinese	8 T. Chinese	9 Korean
	u8 vpatch;	 	// 0 No Video patches	1 Smart Video patching	2 More Video patching	3 Full Video patching
	u8 hook;	 	// 0 No Ocarina&debugger	1 Hooktype: VBI	2 Hooktype: KPAD	3 Hooktype: Joypad	4 Hooktype: GXDraw	5 Hooktype: GXFlush	6 Hooktype: OSSleepThread	7 Hooktype: AXNextFrame
	u8 ocarina; 	// 0 No Ocarina	1 Ocarina from NAND 	2 Ocarina from SD	3 Ocarina from USB"
	u8 bootMode;	// 0 Normal boot method	1 Load apploader
	}
s_channelConfig;

typedef struct
	{
	u8 nand; 	 	// 0 no nand emu	1 sd nand emu	2 usb nand emu
	char nandPath[64];			// Folder of the nand
	s_channelConfig channel;
	}
s_nandbooter;

typedef void (*entrypoint) (void); 
extern void __exception_closeall();

// utils.c
void InitVideo (void);
void printd(const char *text, ...);

bool LoadExecFile (char *path, char *args);
bool BootExecFile (void);

void green_fix(void); //GREENSCREEN FIX
void InitVideo (void);
void printd(const char *text, ...);

void *allocate_memory(u32 size)
{
	void *temp;
	temp = memalign(32, (size+31)&(~31) );
	memset(temp, 0, (size+31)&(~31) );
	return temp;
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

void RestoreSM (int nidx)
	{
	bool find = FALSE;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char pathBack[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char *sm[] = {"00000088","00000085","0000008b","0000008e","00000098","00000095","0000009b","0000009e"};
	s32 smsize;
	bool issm = FALSE;
	
	ISFS_Initialize();
	
	int i;
	s32 fd;
	for (i = 0; i < sizeof(sm) / sizeof(char*); i++)
		{
		sprintf (path, "%s/title/00000001/00000002/content/%s.app", nandConfig->NandInfo[nidx], sm[i]);
		sprintf (pathBack, "%s/title/00000001/00000002/content/%s.bak", nandConfig->NandInfo[nidx], sm[i]);
		
		fd = ISFS_Open (path, ISFS_OPEN_READ);
		Debug ("ReplaceNandSystemMenu: checking %s (%d)", sm[i], fd);

		if (fd < 0) continue;
		smsize = ISFS_Seek(fd, 0, 2);
		
		Debug ("ReplaceNandSystemMenu: sm size %d", smsize);
		
		if (smsize > 1000000) // E' piu' grande di 1MB... e' il sistem menu
			issm = TRUE;
		
		ISFS_Close (fd);
		find = TRUE;
		break;
		}

	if (find && !issm)
		{
		// Restore filesistem
		ISFS_Delete (path);
		ISFS_Rename (pathBack, path);
		}
		
	ISFS_Deinitialize ();
	}

void RestorePriiloader (int nidx)
	{
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char pathBack[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	ISFS_Initialize();

	sprintf (path, "%s/title/00000001/00000002/data/main.bin", nandConfig->NandInfo[nidx]);
	sprintf (pathBack, "%s/title/00000001/00000002/data/main.bak", nandConfig->NandInfo[nidx]);

	ISFS_Delete (path);
	ISFS_Rename (pathBack, path);
		
	sprintf (path, "%s/title/00000001/00000002/data/loader.ini", nandConfig->NandInfo[nidx]);
	sprintf (pathBack, "%s/title/00000001/00000002/data/loader.bak", nandConfig->NandInfo[nidx]);

	ISFS_Delete (path);
	ISFS_Rename (pathBack, path);
		
	ISFS_Deinitialize ();
	}


void Reload (void)
	{
	if (!GetFileToBoot ())
		{
		// run sm, priiloader on the nand is required
		*(vu32*)0x8132FFFB = 0x50756E65;
		DCFlushRange((void*)0x8132FFFB,4);
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0); 
		}
	
	struct __argv arg;
	memset (&arg, 0, sizeof(struct __argv));
	
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

	exit (0);
	}

bool readch (s_channelConfig *cc)
	{
	s32 ret;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	Debug ("neek_PLNandInfo [begin]");
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandcfg.ch");
	
	s32 fd;
	
	fd = ISFS_Open( path, ISFS_OPEN_READ);
	ret = -1;
	if (fd > 0)
		{
		ret = ISFS_Read(fd, cc, sizeof(s_channelConfig));
		ISFS_Close(fd);
		}
	
	ISFS_Deinitialize ();
	
	if (ret >= 0) return true;	
	return false;
	}

void RestoreSneekFolder (void)
	{
	s32 fd;
	int ret;

	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char pathBak[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	Debug ("RestoreSneekFolder [begin]");
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandpath.bin");
	ret = ISFS_Delete (path);
	Debug ("RestoreSneekFolder: delete '%s' = %d", path, ret);

	sprintf (path, "/sneek/nandcfg.pl");
	ret = ISFS_Delete (path);
	Debug ("RestoreSneekFolder: delete '%s' = %d", path, ret);

	ret = sprintf (path, "/sneek/nandcfg.ch");
	ISFS_Delete (path);
	Debug ("RestoreSneekFolder: delete '%s' = %d", path, ret);

	sprintf (path, "/title/00000001/00000002/data/loader.ini");
	sprintf (pathBak, "/title/00000001/00000002/data/loader.bak");
	
	fd = ISFS_Open(pathBak, ISFS_OPEN_READ);
	if (fd > 0)
		{
		ISFS_Close(fd);
		
		ret = ISFS_Delete (path);
		Debug ("RestoreSneekFolder: delete '%s' = %d", path, ret);		
		ret = ISFS_Rename (pathBak, path);
		Debug ("RestoreSneekFolder: rename '%s'->'%s' = %d", pathBak, path, ret);
		}

	sprintf (path, "/sneek/nandcfg.bin");
	sprintf (pathBak, "/sneek/nandcfg.bak");

	fd = ISFS_Open(pathBak, ISFS_OPEN_READ);
	if (fd > 0)
		{
		ISFS_Close(fd);
		
		ret = ISFS_Delete (path);
		Debug ("RestoreSneekFolder: delete '%s' = %d", path, ret);
		ret = 		ISFS_Rename (pathBak, path);
		Debug ("RestoreSneekFolder: rename '%s'->'%s' = %d", pathBak, path, ret);
		}
	
	ISFS_Deinitialize ();
	
	Debug ("RestoreSneekFolder [end]");
	}


//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
	{
	IOS_ReloadIOS(56);
	
	InitVideo ();
	
	printd ("---------------------------------------------------------------------------");
	printd ("                        neekbooter "VER" by stfour");
	printd ("                       (part of postLoader project)");
	printd ("---------------------------------------------------------------------------");

	u32 idx = -1;
	u32 status = 0;
	u32 hi, lo;
	
	if (neek_PLNandInfo	(0, &idx, &status, &lo, &hi) == false)
		{
		printd ("no boot information...");
		Reload ();
		}
	
	printd ("idx = %d", idx);
	printd ("status = %d", status);
	
	if (status == PLNANDSTATUS_NONE)
		{
		status = PLNANDSTATUS_BOOTING;
		neek_PLNandInfo	(1, &idx, &status, &lo, &hi);
	
		if (!hi && !lo)
			{
			printd ("booting disk");
			
			// Copy the di image
			memcpy(EXECUTE_ADDR, di_dol, di_dol_size);
			DCFlushRange((void *) EXECUTE_ADDR, di_dol_size);

			// Load the booter
			memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
			DCFlushRange(BOOTER_ADDR, booter_dol_size);
			
			memset(ARGS_ADDR, 0, sizeof(struct __argv));
			DCFlushRange(ARGS_ADDR, sizeof(struct __argv));
			
			printd ("stating di");

			entrypoint hbboot_ep = (entrypoint) BOOTER_ADDR;

			// bootit !
			u32 level;
			SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
			_CPU_ISR_Disable(level);
			__exception_closeall();
			hbboot_ep();
			_CPU_ISR_Restore(level);
			}
		else
			{
			printd ("booting channel");
			
			s_nandbooter nb ATTRIBUTE_ALIGN(32);

			u8 *tfb = ((u8 *) 0x93200000);
			
			memset (&nb, 0, sizeof (s_nandbooter));

			nb.channel.language = -1;
			nb.channel.titleId = TITLE_ID (hi, lo);
			nb.channel.bootMode = 1;
			
			// Copy the triiforce image
			memcpy(EXECUTE_ADDR, nandbooter_dol, nandbooter_dol_size);
			DCFlushRange((void *) EXECUTE_ADDR, nandbooter_dol_size);

			// Load the booter
			memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
			DCFlushRange(BOOTER_ADDR, booter_dol_size);

			memset(ARGS_ADDR, 0, sizeof(struct __argv));
			DCFlushRange(ARGS_ADDR, sizeof(struct __argv));

			memcpy (tfb, &nb, sizeof(s_nandbooter));
			
			printd ("stating nandbooter");

			entrypoint hbboot_ep = (entrypoint) BOOTER_ADDR;

			u32 level;
			SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
			_CPU_ISR_Disable(level);
			__exception_closeall();
			hbboot_ep();
			_CPU_ISR_Restore(level);
			}
		}
	else if (status == PLNANDSTATUS_BOOTING)
		{
		status = PLNANDSTATUS_BOOTED;
		neek_PLNandInfo	(1, &idx, &status, &lo, &hi);
		
		if (!hi && !lo)
			{
			printd ("restoring old nanndindex");
			
			neek_GetNandConfig ();
			//RestorePriiloader (nandConfig->NandSel);

			// Go back to previous nand
			nandConfig->NandSel = idx;
			neek_WriteNandConfig ();
			
			neek_PLNandInfoRemove ();
			SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
			}
		else
			{
			// restore sneek files
			RestoreSneekFolder ();
			SYS_ResetSystem(SYS_RESTART,0,0);
			}
		}
		
	exit (0);
	}
