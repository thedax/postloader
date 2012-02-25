#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ogc/isfs.h>
#include <ogc/machine/processor.h>
#include "globals.h"
#include "bin2o.h"

extern void __exception_closeall();

void RunLoader(void)
	{
	char argb[300];
	char part[32];
	
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
		sprintf (argb, "boot.dol;%s;partition=%s;intro=0;theme=pl;ios=%d", config.run.asciiId, part, ios);

		DirectDolBoot ("sd://apps/USBLoader_cfg/boot.dol", argb);
		DirectDolBoot ("sd://apps/USBLoader/boot.dol", argb);
		DirectDolBoot ("usb://apps/USBLoader_cfg/boot.dol", argb);
		DirectDolBoot ("usb://apps/USBLoader/boot.dol", argb);
		}
	if (config.run.game.loader == 1) // GX
		{
		sprintf (argb, "boot.dol;%s;ios=%d", config.run.asciiId, ios);

		DirectDolBoot ("sd://apps/usbloader_gx/boot.dol", argb);
		DirectDolBoot ("usb://apps/usbloader_gx/boot.dol", argb);
		}
	if (config.run.game.loader == 2) // WF
		{
		sprintf (argb, "boot.dol;%s;ios=%d", config.run.asciiId, ios);

		DirectDolBoot ("sd://apps/wiiflow/boot.dol", argb);
		DirectDolBoot ("usb://apps/wiiflow/boot.dol", argb);
		}
		
	grlib_menu ("The selected loader wasn't found.\npostLoader will now reset your WII", "OK");

	Shutdown (0);
	SYS_ResetSystem(SYS_RESTART,0,0);
	}