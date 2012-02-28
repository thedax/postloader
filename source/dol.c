// this code was contributed by shagkur of the devkitpro team, thx!

#include <stdio.h>
#include <string.h>

#include <gccore.h>
#include <ogcsys.h>

#include <ogc/machine/processor.h>

#include "wiiload/wiiload.h"
#include "globals.h"
#include "ios.h"
#include "bin2o.h"
#include "hbcstub.h"

extern s32 __IOS_ShutdownSubsystems();
extern void __exception_closeall();

typedef struct _dolheader {
	u32 text_pos[7];
	u32 data_pos[11];
	u32 text_start[7];
	u32 data_start[11];
	u32 text_size[7];
	u32 data_size[11];
	u32 bss_start;
	u32 bss_size;
	u32 entry_point;
} dolheader;

#define MEM1_MAX 0x817FFFFF

u32 load_dol(const void *dolstart, struct __argv *argv)
{
	u32 i;
	dolheader *dolfile;

	if (dolstart)
	{
		dolfile = (dolheader *) dolstart;

		if (dolfile->bss_start < MEM1_MAX) 
			{
			u32 size = dolfile->bss_size;
			if (dolfile->bss_start + dolfile->bss_size > MEM1_MAX) 
				size = MEM1_MAX - dolfile->bss_start;
			
			memset((void *)dolfile->bss_start, 0, size);
			DCFlushRange((void *) dolfile->bss_start, dolfile->bss_size);
			ICInvalidateRange((void *)dolfile->bss_start, dolfile->bss_size);
			}

		for (i = 0; i < 7; i++)
			{
			if ((!dolfile->text_size[i]) || (dolfile->text_start[i] < 0x100))
				continue;

			memmove((void *) dolfile->text_start[i], dolstart + dolfile->text_pos[i], dolfile->text_size[i]);

			DCFlushRange ((void *) dolfile->text_start[i], dolfile->text_size[i]);
			ICInvalidateRange((void *) dolfile->text_start[i], dolfile->text_size[i]);
			}

		for (i = 0; i < 11; i++)
			{
			if ((!dolfile->data_size[i]) || (dolfile->data_start[i] < 0x100))
				continue;

			memmove((void *) dolfile->data_start[i], dolstart + dolfile->data_pos[i], dolfile->data_size[i]);
			DCFlushRange((void *) dolfile->data_start[i], dolfile->data_size[i]);
			}

		if (argv && argv->argvMagic == ARGV_MAGIC)
			{
			void *new_argv = (void *) (dolfile->entry_point + 8);
			memmove(new_argv, argv, sizeof(*argv));
			DCFlushRange(new_argv, sizeof(*argv));
			}
		return dolfile->entry_point;
	}
	return 0;
}

bool NeedArgs (u8 *buffer)
	{
	if (
		buffer[260] == '_' &&
		buffer[261] == 'a' &&
		buffer[262] == 'r' &&
		buffer[263] == 'g')
		return true;
		
	return false;
	}

int DolBootPrepareWiiload (void)
	{
	MasterInterface (1, 0, 3, "Booting...");

	memcpy(EXECUTE_ADDR, wiiload.buff, wiiload.buffsize);
	DCFlushRange((void *) EXECUTE_ADDR, wiiload.buffsize);
	
	struct __argv arg;
	memset (&arg, 0, sizeof(struct __argv));

	if (wiiload.argl)
		{
		arg.argvMagic = ARGV_MAGIC;
		arg.length  = wiiload.argl;
		arg.commandLine = (char*)CMDL_ADDR;
		
		memcpy (arg.commandLine, wiiload.args, arg.length);
		}

	memmove(ARGS_ADDR, &arg, sizeof(arg));
	DCFlushRange(ARGS_ADDR, sizeof(arg) + arg.length);

	mssleep (500);
	
	return 1;
	}

static u8 *execBuffer = NULL;
static size_t filesize;
static struct __argv arg;
static bool fixCrashOnExit;

int DolBootPrepare (s_run *run)
	{
	int i,l;
	char bootpath[PATHMAX+256]; // we need also args...
	char path[PATHMAX];
	
	Video_LoadTheme (0); // Make sure that theme isn't loaded
		
	MasterInterface (1, 0, 2, "Loading DOL...");

	sprintf (path, "%s%s", run->path, run->filename);

	execBuffer = ReadFile2Buffer (path, &filesize, NULL, FALSE);
	if (!execBuffer) return 0;
	
	MasterInterface (1, 0, 3, "Booting...");

	strcpy (bootpath, path);
	strcat (bootpath, ";");

	memset (&arg, 0, sizeof(struct __argv));
	
	if (strlen(run->args) > 0 || NeedArgs(EXECUTE_ADDR))
		{
		arg.argvMagic = ARGV_MAGIC;
		arg.length  = strlen(bootpath)+strlen(run->args)+1;
		arg.commandLine = (char*)CMDL_ADDR;
		
		sprintf(arg.commandLine, "%s%s",bootpath, run->args);
		
		l = strlen(arg.commandLine);
		for (i = 0; i < l; i++)
			if (arg.commandLine[i] == ';')
				arg.commandLine[i] = 0;
		}
		
	fixCrashOnExit = run->fixCrashOnExit;
			
	mssleep (500);

	return 1;
	}
	
void DolBoot (void)
	{
	u32 level;
	
	gprintf ("DolBoot\n");

	Shutdown (fixCrashOnExit);

	memmove(ARGS_ADDR, &arg, sizeof(arg));
	DCFlushRange(ARGS_ADDR, sizeof(arg) + arg.length);
	
	HBMAGIC_ADDR[0] = 'P';
	HBMAGIC_ADDR[1] = 'O';
	HBMAGIC_ADDR[2] = 'S';
	HBMAGIC_ADDR[3] = 'T';
	HBMAGIC_ADDR[4] = fixCrashOnExit;
	DCFlushRange(HBMAGIC_ADDR, 8);

	memcpy(EXECUTE_ADDR, execBuffer, filesize);
	DCFlushRange((void *) EXECUTE_ADDR, filesize);
	free (execBuffer);

	if (config.runHBwithForwarder && vars.neek == NEEK_NONE)
		ReloadPostloaderChannel ();
	
	gprintf ("booter_dol\n");
	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	entrypoint hbboot_ep = (entrypoint) BOOTER_ADDR;
	
	// Try to reload os... maybe this is not needed when NOT executed under homebrew, but a lot of 
	// programs (like WiiMC) aren't able to find any usb device
	gprintf ("reload\n");
	ios_ReloadIOS (-1, NULL);
	
	// Execute dol
	gprintf ("execute\n");
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(level);
	__exception_closeall();
	hbboot_ep();
	_CPU_ISR_Restore(level);
	}
	
bool DirectDolBoot (char *fn, char *arguments)
	{
	s_run run;
	char path[PATHMAX]; 		// Full app path with also the device

	memset (&run, 0, sizeof (s_run));

	strcpy (path, fn);
	
	int i = strlen(path)-1;
	while (i >= 0)
		{
		if (path[i] == '/') break;
		i--;
		}
		
	strcpy (run.filename, &path[i + 1]);
	path[i + 1] = 0;
	strcpy (run.path, path);
	
	if (arguments) sprintf (run.args, arguments);
	
	if (!DolBootPrepare (&run)) return false;
	DolBoot ();
	
	return true;
	}
	
void FastDolBoot (void)
	{
	u32 level;
	
	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	entrypoint hbboot_ep = (entrypoint) BOOTER_ADDR;
	
	if (!HBMAGIC_ADDR[4])
		{
		*(u32*)0x80001804 = (u32) 0L;
		*(u32*)0x80001808 = (u32) 0L;
		DCFlushRange((void*)0x80001804,8);
		}
		
	// Also modify it
	Set_Stub (((u64)(1) << 32) | (2));

	// Execute dol
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(level);
	__exception_closeall();
	hbboot_ep();
	_CPU_ISR_Restore(level);
	}
	
