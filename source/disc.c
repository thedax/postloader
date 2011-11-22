#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ogc/isfs.h>
#include <ogc/machine/processor.h>
#include "globals.h"
#include "bin2o.h"
#include "neek.h"

extern void __exception_closeall();

void CreatePriiloaderSettings (char *nandpath, u8 * iniBuff, s32 iniSize);
void *allocate_memory(u32 size);
//s32 UNEEK_SelectGame( u32 SlotID );

void ReplaceNandSystemMenu (int nidx)
	{
	bool find = FALSE;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char pathBack[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char *sm[] = {"00000088","00000085","0000008b","0000008e","00000098","00000095","0000009b","0000009e"};
	s32 smsize;
	bool issm = FALSE;
	
	char fn[64];

	sprintf (fn, "%s://ploader/n2oswitch.app", vars.defMount);
	FILE *f = fopen(fn, "rb");
	Debug ("ReplaceNandSystemMenu: fopen %x", f);
	if (!f) return;
	fseek( f, 0, SEEK_END);
	long buffsize = ftell(f);
	fseek( f, 0, SEEK_SET);
	char *buff = allocate_memory (buffsize);
	fread(buff, 1, buffsize, f );
	fclose (f);
	
	//IOSPATCH_Apply ();

	ISFS_Initialize();
	
	int i;
	s32 fd;
	for (i = 0; i < sizeof(sm) / sizeof(char*); i++)
		{
		sprintf (path, "%s/title/00000001/00000002/content/%s.app", nandConfig->NandInfo[nidx], sm[i]);
		sprintf (pathBack, "%s/title/00000001/00000002/content/%s.bak", nandConfig->NandInfo[nidx], sm[i]);
		
		fd = ISFS_Open (path, ISFS_OPEN_READ);
		Debug ("ReplaceNandSystemMenu: checking %s (%d)", path, fd);

		if (fd < 0) continue;
		smsize = ISFS_Seek(fd, 0, 2);
		
		Debug ("ReplaceNandSystemMenu: sm size %d", smsize);
		
		if (smsize > 400000) // E' piu' grande di 1MB... e' il sistem menu
			issm = TRUE;
		
		ISFS_Close (fd);
		find = TRUE;
		break;
		}
		
	if (find && issm)
		{
		s32 ret;
		
		ret = ISFS_Delete (pathBack);
		Debug ("ReplaceNandSystemMenu: ISFS_Delete %s (%d)", pathBack, ret);

		ret = ISFS_Rename (path, pathBack);
		Debug ("ReplaceNandSystemMenu: ISFS_Rename %s %s (%d)", path, pathBack, ret);
		
		ISFS_CreateFile (path,0,3,3,3);
		fd = ISFS_Open (path, ISFS_OPEN_RW);
		Debug ("ReplaceNandSystemMenu: ISFS_Open %s (%d)", path, fd);
		
		ret = ISFS_Write (fd, buff, buffsize);
		Debug ("ReplaceNandSystemMenu: ISFS_Write %s (%d)", path, ret);
		
		ret = ISFS_Close (fd);
		Debug ("ReplaceNandSystemMenu: ISFS_Close %s (%d)", path, ret);
		}
	/*
	if (find && !issm)
		{
		// Restore filesistem
		ISFS_Delete (path);
		ISFS_Rename (pathBack, path);
		}
	*/
	ISFS_Deinitialize ();
	}

void SetupPriiloader (int nidx)
	{
	bool find = FALSE;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char pathBack[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char *sm[] = {"10000088","10000085","1000008b","1000008e","10000098","10000095","1000009b","1000009e"};
	char fn[64];

	sprintf (fn, "%s://ploader/n2oswitch.dol", vars.defMount);
	FILE *f = fopen(fn, "rb");
	Debug ("SetupPriiloader: fopen %x", f);
	if (!f) return;
	fseek( f, 0, SEEK_END);
	long buffsize = ftell(f);
	fseek( f, 0, SEEK_SET);
	char *buff = allocate_memory (buffsize);
	fread(buff, 1, buffsize, f );
	fclose (f);
	
	ISFS_Initialize();
	
	int i;
	s32 fd;
	for (i = 0; i < sizeof(sm) / sizeof(char*); i++)
		{
		sprintf (path, "%s/title/00000001/00000002/content/%s.app", nandConfig->NandInfo[nidx], sm[i]);
		
		fd = ISFS_Open (path, ISFS_OPEN_READ);
		Debug ("SetupPriiloader: checking %s (%d)", path, fd);
		if (fd < 0) continue;
		ISFS_Close (fd);
		find = TRUE;
		break;
		}
		
	if (find)
		{
		s32 ret;
		s32 loaderIniSize = 0;
		u8 * loaderIniBuff = NULL;
		
		// Save priiloader settings
		
		sprintf (path, "%s/title/00000001/00000002/data/main.bin", nandConfig->NandInfo[nidx]);
		sprintf (pathBack, "%s/title/00000001/00000002/data/main.bak", nandConfig->NandInfo[nidx]);

		ret = ISFS_Delete (pathBack);
		Debug ("SetupPriiloader: ISFS_Delete %s (%d)", pathBack, ret);
		ret = ISFS_Rename (path, pathBack);
		Debug ("SetupPriiloader: ISFS_Rename %s %s (%d)", path, pathBack, ret);
		
		sprintf (path, "%s/title/00000001/00000002/data/loader.ini", nandConfig->NandInfo[nidx]);
		sprintf (pathBack, "%s/title/00000001/00000002/data/loader.bak", nandConfig->NandInfo[nidx]);

		// Get priiloader loader.ini files contents
		fd = ISFS_Open (path, ISFS_OPEN_READ);
		if (fd >= 0)
			{
			loaderIniSize = ISFS_Seek(fd, 0, 2);
			
			if (loaderIniSize)
				{
				loaderIniBuff = allocate_memory (loaderIniSize);
				ISFS_Seek(fd, 0, 0);
				ISFS_Read(fd, loaderIniBuff, loaderIniSize);
				}
			
			ISFS_Close (fd);
			}

		ret = ISFS_Delete (pathBack);
		Debug ("SetupPriiloader: ISFS_Delete %s (%d)", pathBack, ret);
		ret = ISFS_Rename (path, pathBack);
		Debug ("SetupPriiloader: ISFS_Rename %s %s (%d)", path, pathBack, ret);

		// Store n2oswitch
		sprintf (path, "%s/title/00000001/00000002/data/main.bin", nandConfig->NandInfo[nidx]);
		ISFS_CreateFile (path,0,3,3,3);
		fd = ISFS_Open (path, ISFS_OPEN_RW);
		ret = ISFS_Write (fd, buff, buffsize);
		Debug ("SetupPriiloader: ISFS_Write (%d %d)", ret, buffsize);
		ret = ISFS_Close (fd);
		
		CreatePriiloaderSettings ((char*)nandConfig->NandInfo[nidx], loaderIniBuff, loaderIniSize);
		free (loaderIniBuff);
		}
	ISFS_Deinitialize ();
	}


void Disc(void)
	{
	if (vars.neek != NEEK_NONE)
		{
		MasterInterface (1, 0, TEX_DVD, "Mounting disc... (slot %d)", config.run.neekSlot);
		neek_SelectGame( config.run.neekSlot );
		sleep (1);
		}
		
	if (config.run.game.ios == 0)
		MasterInterface (1, 0, TEX_DVD, "Starting disc...");
	else
		MasterInterface (1, 0, TEX_DVD, "Switching NAND and Starting disc...");
	
	if (vars.neek == NEEK_2o && config.run.game.ios > 0 && nandConfig)
		{
		u32 idx = nandConfig->NandSel;
		u32 status = PLNANDSTATUS_NONE;
		char nand[64];
		
		neek_PLNandInfo (1, &idx, &status);
		
		if (config.run.game.ios == 1)
			strcpy (nand, "pl_us");

		if (config.run.game.ios == 2)
			strcpy (nand, "pl_eu");
		
		if (config.run.game.ios == 3)
			strcpy (nand, "pl_jp");
		
		if (config.run.game.ios == 4)
			strcpy (nand, "pl_kr");
		
		int ni = -1;
		if (strlen(nand)) 
			ni = neek_NandConfigSelect (nand);
			
		// Replace system menu with 
		if (ni >= 0)
			SetupPriiloader (ni);
			//ReplaceNandSystemMenu (ni);
		
		Shutdown (0);
		
		//SYS_ResetSystem(SYS_RESTART,0,0);
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
		
		exit (0);
		
		/*
		IOS_ReloadIOS(56);
		exit (0);
		*/
		//SYS_ResetSystem(SYS_RESTART,0,0);
		}
		

	// Copy the di image
	memcpy(EXECUTE_ADDR, di_dol, di_dol_size);
	DCFlushRange((void *) EXECUTE_ADDR, di_dol_size);

	// Load the booter
	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	entrypoint hbboot_ep = (entrypoint) BOOTER_ADDR;
	
	// Shutdown all system
	grlibSettings.doNotCall_GRRLIB_Exit = true;
	Shutdown (0);
	
	// bootit !
	u32 level;

	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(level);
	__exception_closeall();
	hbboot_ep();
	_CPU_ISR_Restore(level);	
	}