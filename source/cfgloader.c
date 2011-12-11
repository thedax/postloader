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

void CFGLoader(void)
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
		
	MasterInterface (1, 0, TEX_DVD, "Starting game\nPowered by Configurable USB Loader...");
	sleep (1);
	
	buff = CFGSearch (&filesize);
	if (buff);

	// Copy the di image
	memcpy(EXECUTE_ADDR, buff, filesize);
	DCFlushRange((void *) EXECUTE_ADDR, filesize);

	// Load the booter
	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);
	
	memset (argb, 0, sizeof(argb));
	sprintf (argb, "boot.dol;%s;partition=%s;intro=0;theme=pl", config.run.asciiId, part);
	
	struct __argv arg;
 	memset (&arg, 0, sizeof(struct __argv)); 
	
	arg.argvMagic = ARGV_MAGIC;
	arg.length  = strlen(argb)+1;
	arg.commandLine = (char*)CMDL_ADDR;
	
	int i, l;
	l = strlen(argb);
	for (i = 0; i < l; i++)
		if (argb[i] == ';')
			argb[i] = '\0';
	
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