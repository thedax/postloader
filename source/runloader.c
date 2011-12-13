#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ogc/isfs.h>
#include <ogc/machine/processor.h>
#include "globals.h"
#include "bin2o.h"

extern void __exception_closeall();

static u8 *CFGSearch (size_t *filesize)
	{
	u8 *buff = NULL;
	
	if (!buff) buff = ReadFile2Buffer ("sd://apps/USBLoader_cfg/boot.dol", filesize, NULL, 1);
	if (!buff) buff = ReadFile2Buffer ("sd://apps/USBLoader/boot.dol", filesize, NULL, 1);
	if (!buff) buff = ReadFile2Buffer ("usb://apps/USBLoader_cfg/boot.dol", filesize, NULL, 1);
	if (!buff) buff = ReadFile2Buffer ("usb://apps/USBLoader/boot.dol", filesize, NULL, 1);
	
	return buff;
	}

static u8 *GXSearch (size_t *filesize)
	{
	u8 *buff = NULL;
	
	if (!buff) buff = ReadFile2Buffer ("sd://apps/usbloader_gx/boot.dol", filesize, NULL, 1);
	if (!buff) buff = ReadFile2Buffer ("usb://apps/usbloader_gx/boot.dol", filesize, NULL, 1);
	
	return buff;
	}

static u8 *WFSearch (size_t *filesize)
	{
	u8 *buff = NULL;
	
	if (!buff) buff = ReadFile2Buffer ("sd://apps/wiiflow/boot.dol", filesize, NULL, 1);
	if (!buff) buff = ReadFile2Buffer ("usb://apps/wiiflow/boot.dol", filesize, NULL, 1);
	
	return buff;
	}

void RunLoader(void)
	{
	u8 *buff = NULL;
	char argb[300];
	char part[32];
	
	if (config.run.neekSlot < 10)
		sprintf (part, "FAT%d", config.run.neekSlot + 1);
	else
		sprintf (part, "NTFS%d", config.run.neekSlot - 10 + 1);
	
	size_t filesize;

	if (vars.neek != NEEK_NONE)
		return;
		
	if (config.run.game.loader == 0)
		{
		MasterInterface (1, 0, TEX_DVD, "Starting game\nPowered by Configurable USB Loader...");
		buff = CFGSearch (&filesize);
		}
	if (config.run.game.loader == 1)
		{
		MasterInterface (1, 0, TEX_DVD, "Starting game\nPowered by USB Loader GX...");
		buff = GXSearch (&filesize);
		}
	if (config.run.game.loader == 2)
		{
		MasterInterface (1, 0, TEX_DVD, "Starting game\nPowered by WiiFlow...");
		buff = WFSearch (&filesize);
		}
	
	//grlib_dosm ("size = %d", filesize);
	if (buff == NULL)
		{
		grlib_menu ("The selected loader wasn't found.\npostLoader will now reset your WII", "OK");

		Shutdown (0);
		SYS_ResetSystem(SYS_RESTART,0,0);
		}
	
	sleep (1);
	
	// Copy the di image
	memcpy(EXECUTE_ADDR, buff, filesize);
	DCFlushRange((void *) EXECUTE_ADDR, filesize);

	// Load the booter
	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);
	
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
		memset (argb, 0, sizeof(argb));
		sprintf (argb, "boot.dol;%s;partition=%s;intro=0;theme=pl;ios=%d", config.run.asciiId, part, ios);
		}
	if (config.run.game.loader == 1) // GX
		{
		memset (argb, 0, sizeof(argb));
		sprintf (argb, "boot.dol;%s;ios=%d", config.run.asciiId, ios);
		}
	if (config.run.game.loader == 2) // WF
		{
		memset (argb, 0, sizeof(argb));
		sprintf (argb, "boot.dol;%s;ios=%d", config.run.asciiId, ios);
		}
	
	struct __argv arg;
 	memset (&arg, 0, sizeof(struct __argv)); 
	
	if (strlen(argb))
		{
		arg.argvMagic = ARGV_MAGIC;
		arg.length  = strlen(argb)+1;
		arg.commandLine = (char*)CMDL_ADDR;
		
		int i;
		for (i = 0; i < arg.length ; i++)
			if (argb[i] == ';')
				argb[i] = '\0';
		}
	
	memcpy (arg.commandLine, argb, arg.length);

	memmove(ARGS_ADDR, &arg, sizeof(arg));
	DCFlushRange(ARGS_ADDR, sizeof(arg) + arg.length);
 
	entrypoint hbboot_ep = (entrypoint) BOOTER_ADDR;
	
	// Shutdown all system
	//grlibSettings.doNotCall_GRRLIB_Exit = true;
	Shutdown (0);
	
	// bootit !
	u32 level;

	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(level);
	__exception_closeall();
	hbboot_ep();
	_CPU_ISR_Restore(level);	
	}