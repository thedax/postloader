#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <gccore.h>
#include <dirent.h>
#include <sys/unistd.h>
#include <ogc/machine/processor.h>
#include "neek.h"
#include "bin2o.h"

#define VER "[1.0]"

#define EXECUTE_ADDR    ((u8 *) 0x92000000)
#define BOOTER_ADDR     ((u8 *) 0x93000000)
#define ARGS_ADDR       ((u8 *) 0x93200000) 
#define CMDL_ADDR       ((u8 *) 0x93200000+sizeof(struct __argv))

#define TITLE_ID(x,y)		(((u64)(x) << 32) | (y))

#define Debug printd

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

//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
	{
	InitVideo ();
	
	printd ("---------------------------------------------------------------------------\n");
	printd ("                        n2oswitch "VER" by stfour\n");
	printd ("                       (part of postLoader project)\n");
	printd ("---------------------------------------------------------------------------\n");

	u32 idx = -1;
	u32 status = 0;
	u32 hi, lo;
	
	neek_PLNandInfo	(0, &idx, &status, &lo, &hi);
	printd ("idx = %d", idx);
	printd ("status = %d", status);
	
	if (status == PLNANDSTATUS_NONE)
		{
		status = PLNANDSTATUS_BOOTING;
		neek_PLNandInfo	(1, &idx, &status, &lo, &hi);
	
		if (!hi && !lo)
			{
			// Copy the di image
			memcpy(EXECUTE_ADDR, di_dol, di_dol_size);
			DCFlushRange((void *) EXECUTE_ADDR, di_dol_size);

			// Load the booter
			memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
			DCFlushRange(BOOTER_ADDR, booter_dol_size);
			
			memset(ARGS_ADDR, 0, sizeof(struct __argv));
			DCFlushRange(ARGS_ADDR, sizeof(struct __argv));

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
			WII_Initialize();
			WII_LaunchTitle(TITLE_ID(hi, lo));
			}
		}
	else if (status == PLNANDSTATUS_BOOTING)
		{
		neek_GetNandConfig ();
		RestorePriiloader (nandConfig->NandSel);

		status = PLNANDSTATUS_BOOTED;
		neek_PLNandInfo	(1, &idx, &status, &lo, &hi);
		
		if (!hi && !lo)
			{
			// Go back to previous nand
			nandConfig->NandSel = idx;
			neek_WriteNandConfig ();
			
			SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
			}
		}
		
	exit (0);
	}
