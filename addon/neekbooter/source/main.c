/*

priiBooter is a small programm to be added to priiloader. It allow to spawn anoteher program

*/


#include <gccore.h>
#include <ogc/machine/processor.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>
#include "../build/bin2o.h"

#define EXECUTE_ADDR    ((u8 *) 0x92000000)
#define BOOTER_ADDR     ((u8 *) 0x93000000)
#define ARGS_ADDR       ((u8 *) 0x93200000)
#define CMDL_ADDR       ((u8 *) 0x93200000+sizeof(struct __argv))

#define NEEK 0

extern void __exception_closeall();
typedef void (*entrypoint) (void); 

u32 cookie;
void *exeBuffer = (void *) EXECUTE_ADDR;

#define CFG_HOME_WII_MENU   0x50756E65
#define MAXFILESIZE 16777216 // 16mb

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

bool GetFileToBoot (void)
	{
	bool ret = false;
	
	if (fatMountSimple("fat", &__io_wiisd))
		{
		if (!NEEK) ret = LoadFile("fat://plneek.dol");

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
	struct __argv arg;

	if (!GetFileToBoot ())
		{
		// Let's try to start system menu... use magic word, as we may have priiloader behind
		*(vu32*)0x8132FFFB = CFG_HOME_WII_MENU;
		DCFlushRange((void*)0x8132FFFB,4);
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
		return 0;
		}
		
	memset (&arg, 0, sizeof(struct __argv));
	
	if (NEEK)
		{
		char buff[32];

		memset (buff, 0, sizeof(buff));
		strcpy (buff, "neek");

		arg.argvMagic = ARGV_MAGIC;
		arg.length  = strlen(buff)+1;
		arg.commandLine = (char*)CMDL_ADDR;
		strcpy(arg.commandLine, buff);
		}

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
	return 0;
	}