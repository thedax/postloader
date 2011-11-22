#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <malloc.h>
#include <string.h> 
#include "../build/bin2o.h"

#define EXECUTE_ADDR    ((u8 *) 0x92000000)
#define BOOTER_ADDR     ((u8 *) 0x93000000)
#define ARGS_ADDR       ((u8 *) 0x93200000) 
#define CMDL_ADDR       ((u8 *) 0x93200000+sizeof(struct __argv))
#define MAXFILESIZE 16777216 // 16mb

void green_fix(void); //GREENSCREEN FIX

static u32 cookie;
static void *exeBuffer = (void *) EXECUTE_ADDR; 

extern void __exception_closeall();
typedef void (*entrypoint) (void);  

static entrypoint exeEntryPoint;

bool LoadExecFile (char *path, char *args)
	{
	FILE *f;
	size_t size;
	
	f = fopen(path, "rb");
	if (!f) return false;
	
	size = fread(exeBuffer, 1, MAXFILESIZE, f );
	fclose (f);
	
	if (size <= 0) return false;

	struct __argv arg;
 	memset (&arg, 0, sizeof(struct __argv)); 
	
	if (args != NULL)
		{
		arg.argvMagic = ARGV_MAGIC;
		arg.length  = strlen(args) + 1;
		arg.commandLine = (char*)CMDL_ADDR;
		memset (arg.commandLine, 0, arg.length);
		memcpy (arg.commandLine, args, arg.length);
		}
	
	memmove(ARGS_ADDR, &arg, sizeof(arg));
	DCFlushRange(ARGS_ADDR, sizeof(arg) + arg.length);

	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	exeEntryPoint = (entrypoint) BOOTER_ADDR;
	
	return true;
	}

bool BootExecFile (void)
	{
	green_fix ();
	
	/* cleaning up and load dol */
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable (cookie);
	__exception_closeall ();
	exeEntryPoint ();
	_CPU_ISR_Restore (cookie); 

	return true;
	}
