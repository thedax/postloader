#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ogc/isfs.h>
#include <ogc/machine/processor.h>
#include "globals.h"
#include "bin2o.h"
#include "neek.h"

extern void __exception_closeall();
bool SetupNeek2o (void);

static void PrepareNeek2o(void)
	{
	char target[256];
	
	SetupNeek2o ();
	
	// Create configuration file for n2oswitch ...
	u32 data[8];
	data[0] = 0;
	data[1] = PLNANDSTATUS_NONE;
	data[2] = 0;
	data[3] = 0;
	data[4] = 1; // Force return to real
	sprintf (target, "%s/nandcfg.pl", NEEK2O_SNEEK);
	fsop_WriteFile (target, (u8*) data, sizeof(data));

	// Boot neek2o
	Neek2oLoadKernel ();
	Shutdown ();
	Neek2oBoot ();
	}

void RunLoader(void)
	{
	char argb[300];
	char part[32];
	
	if (config.run.game.loader == 3) // NEEK2O
		{
		//neek_CreateCDIConfig (config.run.asciiId);
		if (neek_ReadAndSelectCDIGame (config.run.asciiId))
			PrepareNeek2o ();
		else
			{
			grlib_menu ("The game was not found in diconfig.bin.\nRun neek2o once to update gamelist and try again\npostLoader will now try to reload or reset your WII", "OK");
			
			ReloadPostloader ();
			
			Shutdown ();
			SYS_ResetSystem(SYS_RESTART,0,0);
			}
		}
		
	if (config.run.neekSlot < 10)
		sprintf (part, "FAT%d", config.run.neekSlot + 1);
	else
		sprintf (part, "NTFS%d", config.run.neekSlot - 10 + 1);
	
	int ios;
	if (config.run.game.ios == 0) ios = 249;
	if (config.run.game.ios == 1) ios = 250;
	if (config.run.game.ios == 2) ios = 222;
	if (config.run.game.ios == 3) ios = 223;
	if (config.run.game.ios == 4) ios = 248;
	if (config.run.game.ios == 5) ios = 251;
	if (config.run.game.ios == 6) ios = 252;
	
	if (config.run.game.loader == 0) // CFG
		{
		sprintf (argb, "%s;partition=%s;intro=0;theme=pl;ios=%d", config.run.asciiId, part, ios);

		DirectDolBoot ("sd://apps/USBLoader/boot.dol", argb, 0);
		DirectDolBoot ("sd://apps/USBLoader_cfg/boot.dol", argb, 0);
		DirectDolBoot ("usb://apps/USBLoader/boot.dol", argb, 0);
		DirectDolBoot ("usb://apps/USBLoader_cfg/boot.dol", argb, 0);
		}
	if (config.run.game.loader == 1) // GX
		{
		sprintf (argb, "%s;ios=%d", config.run.asciiId, ios);

		DirectDolBoot ("sd://apps/usbloader_gx/boot.dol", argb, 0);
		DirectDolBoot ("usb://apps/usbloader_gx/boot.dol", argb, 0);
		}
	if (config.run.game.loader == 2) // WF
		{
		sprintf (argb, "%s;ios=%d", config.run.asciiId, ios);

		DirectDolBoot ("sd://apps/wiiflow/boot.dol", argb, 0);
		DirectDolBoot ("usb://apps/wiiflow/boot.dol", argb, 0);
		}
		
	grlib_menu ("The selected loader wasn't found.\npostLoader will now try to reload or reset your WII", "OK");
	
	ReloadPostloader ();
	
	Shutdown ();
	SYS_ResetSystem(SYS_RESTART,0,0);
	}