#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>

#define MAIN // Define global variabiles

#include "wiiload/wiiload.h"
#include "globals.h"
#include "ios.h"
#include "stub.h"
#include "identify.h"
#include "neek.h"
#include "uneek_fs.h"
#include "mem2.h"
#include "channels.h"
#include "bin2o.h"
#include "devices.h"
#include "language.h"

extern void __exception_setreload(int t); // In the event of a code dump, app will restart
int Disc (void);
bool plneek_GetNandName (void);

void BootToSystemMenu(void)
	{
	Shutdown ();
	
	// Let's try to start system menu... use magic word, as we may have priiloader behind
	*(vu32*)0x8132FFFB = PRIILOADER_TOMENU;
	DCFlushRange((void*)0x8132FFFB,4);
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	}

void BootToHBC(void)
	{
	Shutdown ();

	WII_Initialize();

	WII_LaunchTitle(TITLE_ID(0x00010001,0x4C554C5A)); // LULZ
	WII_LaunchTitle(TITLE_ID(0x00010001,0xAF1BF516)); // HBC v1.0.7+
	WII_LaunchTitle(TITLE_ID(0x00010001,0x4a4f4449)); // HBC JODI
	WII_LaunchTitle(TITLE_ID(0x00010001,0x48415858)); // HBC HAXX
	
	exit (0);
	}

void WiiLoad (int start)
	{
#ifndef DOLPHINE
	if (start)
		WiiLoad_Start (vars.tempPath, 5);
	else
		WiiLoad_Stop ();
#endif
	}
	
void SetupCoversPaths (void)
	{
	*vars.covers = '\0';
	*vars.coversEmu = '\0';
	
	char path[64];
	sprintf (path, "%s://ploader/paths.conf", vars.defMount);
	char *buff = (char*)fsop_ReadFile (path, 0, NULL);
	if (buff)
		{
		char *p;
		
		p = cfg_FindInBuffer (buff, "covers");
		if (p) strcpy (vars.covers, p);
		
		p = cfg_FindInBuffer (buff, "covers.emu");
		strcpy (vars.coversEmu, p);
		
		free (buff);
		}

	if (!(*vars.covers)) sprintf (vars.covers, "%s://ploader/covers", vars.defMount);
	if (!(*vars.coversEmu)) sprintf (vars.coversEmu, "%s://ploader/covers.emu", vars.defMount);
	
	//char *d;

	Debug ("vars.covers = '%s'", vars.covers);
	/*
	d = fsop_GetDirAsString (vars.covers, '\n', 0, NULL);
	if (d)
		{
		Debug (d);
		free (d);
		}
	*/
	Debug ("vars.covers.emu = '%s'", vars.coversEmu);
	/*
	d = fsop_GetDirAsString (vars.coversEmu, '\n', 0, NULL);
	if (d)
		{
		Debug (d);
		free (d);
		}
	*/
	}
	
void Subsystems (bool enable)
	{
	if (enable)
		{
		if (vars.neek == NEEK_NONE) USB_Initialize ();
		mt_Init ();
		grlib_Controllers (true);
		MountDevices (1);
		DebugStart (true, "sd://ploader.log");
		WiiLoad (1);
		CoverCache_Start ();
		}
	else
		{
		ConfigWrite ();
		CoverCache_Stop ();
		WiiLoad (0);
		Debug ("stopping controllers");
		grlib_Controllers (false);
		Debug ("stopping debug");
		DebugStop ();
		UnmountDevices ();
		mt_Deinit ();
		if (vars.neek == NEEK_NONE) USB_Deinitialize ();
		}
	}

void Shutdown(void)
	{
	Debug ("Shutdown !");
	
	snd_Stop ();
	free (vars.titlestxt);

	// Load proper stub
	StubLoad ();
	
	Subsystems (false);
	Video_Deinit ();
	
	if (vars.neek != NEEK_NONE) // We are not working under neek
		{
		// exit_uneek_fs ();
		}
	}

int Initialize (int silent)
	{
	InitMem2Manager ();
	Video_Init ();
	
	// Prepare the background
	if (!silent) 
		{
		MasterInterface (1, 0, TEX_HGL, "Please wait...");
		MasterInterface (1, 0, TEX_HGL, "Please wait...");
		}

	int ret = MountDevices (silent);
	grlibSettings.usesGestures = config.usesGestures;
	
	DebugStart (true, "sd://ploader.log");

	Debug ("-[postLoader "VER"]----------------------------------------");
	
	int i;
	for (i = 0; i < DEV_MAX; i++)
		{
		Debug ("Device %d->%s", i, devices_Get (i));
		}
		
	Debug ("Default device is '%s'", vars.defMount);
	
	return ret;
	}
	
static void Redraw (void)
	{
	Video_DrawBackgroud (0);
	Video_SetFont(TTFNORM);
	grlib_printf (320, 440, GRLIB_ALIGNCENTER, 0, "postLoader"VER" (c) 2011-13 by stfour");
	}
	
int MasterInterface (int full, int showCursor, int icon, const char *text, ...)
	{
	static char mex[256] = {0};
	char *p;
	u32 btn;
	
	if (text != NULL)
		{
		va_list argp;
		va_start(argp, text);
		vsprintf(mex, text, argp);
		va_end(argp);
		}
	
	btn = grlib_GetUserInput();
	if (btn & WPAD_BUTTON_A) return 1;
	if (btn & WPAD_BUTTON_B) return 2;
	
	if (full)
		{
		Redraw ();
		grlib_PushScreen ();
		}
	else
		grlib_PopScreen ();
	
	p = strstr (mex, "\n");
	if (p != NULL)
		{
		*p = 0;
		p++;
		}
		
	Video_SetFont(TTFSMALL);
	grlib_Text (320, 400, GRLIB_ALIGNCENTER, 0, mex);
	if (p) grlib_Text (320, 420, GRLIB_ALIGNCENTER, 0, p);

	Video_DrawIcon (icon, 320,360);
	if (showCursor) grlib_DrawIRCursor ();
	
	grlib_Render();  // Render the frame buffer to the TV
	
	return 0;
	}
	
bool SenseSneek (bool isfsinit)
	{
	if (isfsinit) ISFS_Initialize ();
	
	u32 num = 0;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);

	strcpy (path, "/SNEEK");
	s32 ret = ISFS_ReadDir(path, NULL, &num);
	
	if (isfsinit) ISFS_Deinitialize ();
	
	return (ret == 0);
	}
	
void CheckNeek (void)
	{
#ifdef DOLPHINE	
	vars.neek = 1;
	vars.usbtime = 1;
	return;
#endif

	vars.usbtime = 15;

	if (SenseSneek(true)) 
		{
		vars.neek = NEEK_USB;
		//vars.usbtime = 1;
		vars.ios = ios_ReloadIOS (IOS_SNEEK, &vars.ahbprot); // Let's try to reload
		
		//init_uneek_fs (ISFS_OPEN_READ|ISFS_OPEN_WRITE);
		}
	}
	
int main(int argc, char **argv) 
	{
	if (usb_isgeckoalive (EXI_CHANNEL_1))
		{
		gprintf ("\nPL"VER"\n");
		gprintf ("MAGIC: %c-%c-%c-%c\n", HBMAGIC_ADDR[0], HBMAGIC_ADDR[1], HBMAGIC_ADDR[2], HBMAGIC_ADDR[3]);
		}

	int i;
	int ret;
	
	__exception_setreload(3);

	ExtConfigRead ();
	
	memset (&vars, 0, sizeof(s_vars));
	
	get_msec (true);
	
	SetLangFileBuffer ((const char*)english_txt, english_txt_size);
	
	CheckNeek ();
	
	for (i = 0; i < argc; i++)
		{
		if (strstr(argv[i], "neek"))
			vars.neek = NEEK_USB;

		if (strstr(argv[i], "priibooter"))
			vars.usbtime = 1;
		}

#ifndef DOLPHINE
	if (vars.neek == NEEK_NONE) // We are not working under neek
		{
		if (extConfig.use249)
			vars.ios = ios_ReloadIOS (IOS_CIOS, &vars.ahbprot);
		else
			vars.ios = ios_ReloadIOS (-1, &vars.ahbprot); // Let's try to reload
		}
#endif

	if (vars.neek != NEEK_NONE) // We are working under neek
		vars.ios = IOS_GetVersion();
		
	gprintf ("vars.usbtime = %d\n", vars.usbtime);

	ret = Initialize((vars.usbtime == 1) ? 1:0);
	
	// This is just for testing... no really needed
	// Identify_SU (0);
	
	/* Prepare random seed */
	srand(time(0)); 
	grlib_SetRedrawCallback (Redraw, NULL);
	
	if (!ret)
		{
		char buff[100];
		char mex[500];
		
		strcpy (buff, "Boot to WiiMenu##0");
		
		if (devices_Get(DEV_SD)) strcat (buff, "|Create on SD and start postLoader##1");
		if (devices_Get(DEV_USB)) strcat (buff, "|Create on USB and start postLoader##2");
		
		sprintf (mex, "Welcome to postLoader"VER"\n\n"
						"No configuration file was found.\n\n"
						"postLoader need a device to keep it's configuration data,\n"
						"so an SD or an USB (FAT/NTFS) device is needed and one must\n"
						"be selected to store postLoader data\n"
						"IOS = %d", vars.ios);
		
		int item = grlib_menu (0, mex, buff);
		
		if (item <= 0)
			*vars.defMount = '\0';								
		
		if (grlibSettings.wiiswitch_poweroff)
			{
			Shutdown ();
			SYS_ResetSystem( SYS_POWEROFF, 0, 0 );
			}
		
		if (item <= 0 || grlibSettings.wiiswitch_reset)
			{
			BootToSystemMenu ();
			return (0);
			}
			
		if (item == 1)
			SetDefMount (DEV_SD);
		if (item == 2)
			SetDefMount (DEV_USB);
		
		if (item > 0)
			{
			CheckForPostLoaderFolders ();
		
			if (ConfigWrite())
				{
				grlib_menu (0, "Configuration file created succesfully", "OK");
				}
			}
		}
		
	DumpFolders ("//");
	DumpFolders ("//wbfs");
	
	Video_LoadTheme (1);

	if (vars.usbtime == 1)
		for (i = 0; i < 255; i+=16) Video_Predraw (i);
		
	if (vars.neek && neek_IsNeek2o())
		{
		Debug ("neek2o detected");
		vars.neek = NEEK_2o;
		
		// retriving neek nand name
		neek_GetNandConfig ();
		if (nandConfig)
			{
			Debug ("neek2o = 0x%X, %d, %d", nandConfig, nandConfig->NandSel, nandConfig->NandCnt);
			if (nandConfig && nandConfig->NandSel < nandConfig->NandCnt)
				sprintf (vars.neekName, "%s", nandConfig->NandInfo[nandConfig->NandSel]);
			}
		else
			{
			strcpy (vars.neekName,"root");
			}
		
		Debug ("neek2o nand = %s", vars.neekName);
		
		neek_PLNandInfoKill ();
		}
	else
		{
		RestoreChannelNeek2o();
		}

	Debug ("neek = %d", vars.neek);
	if (vars.neek != NEEK_NONE && vars.neek != NEEK_2o)
		plneek_GetNandName ();
	
	Debug ("Autoboot = %d", config.autoboot.enabled);
	if (!config.autoboot.enabled) vars.interactive = 1;

	if (!vars.interactive)
		{
		Debug ("Querying for interactive mode");
		
		time_t t,tout;
		
		t = time(NULL);

		MasterInterface (1, 0, 1, " ");
		
		if (time(NULL) - t > 5)
			tout = time(NULL) + 1;
		else
			tout = time(NULL) + 3;
		
		while (time(NULL) < tout)
			{
			if (MasterInterface (0, 0, 2, "Press (A) to enter in interactive mode...") == 1) vars.interactive = 1;
			mssleep (10);
			
			if (vars.interactive || (grlibSettings.wiiswitch_poweroff || grlibSettings.wiiswitch_reset)) break;
			}
		}
		
	
	sprintf (vars.tempPath, "%s://ploader/temp", vars.defMount);
	Debug ("vars.tempPath = %s", vars.tempPath);
	
	grlibSettings.autoCloseMenu = 60;
	
	ret = INTERACTIVE_RET_NONE;
	if (!((grlibSettings.wiiswitch_poweroff || grlibSettings.wiiswitch_reset)) && ret != INTERACTIVE_RET_WIIMENU && vars.interactive)
		{
		Debug ("Showing gui....");
		devices_WakeUSBWrite ();
		CoverCache_Start ();
		snd_Init ();
		WiiLoad (1);
		neek_UID_Read ();
		SetupCoversPaths ();

		if (vars.usbtime != 1)
			{
			for (i = 0; i < 255; i+=16) Video_Predraw (i);
			}
		
		do
			{
			if (config.browseMode == BROWSE_HB) ret = AppBrowser ();
			if (config.browseMode == BROWSE_CH) ret = ChnBrowser ();
			if (config.browseMode == BROWSE_GM) ret = GameBrowser ();
			if (config.browseMode == BROWSE_EM) ret = EmuBrowser ();
			
			if (ret == INTERACTIVE_RET_TOCHANNELS) 
				config.browseMode = BROWSE_CH;
			else if (ret == INTERACTIVE_RET_TOHOMEBREW)
				config.browseMode = BROWSE_HB;
			else if (ret == INTERACTIVE_RET_TOGAMES)
				config.browseMode = BROWSE_GM;
			else if (ret == INTERACTIVE_RET_TOEMU)
				config.browseMode = BROWSE_EM;
			else if (ret == INTERACTIVE_RET_NEEK2O)
				{
				if (Neek2oLoadKernel ())
					break;
				else
					grlib_menu (50, "Error!\npostLoader was unable to load neek2o kernel", "OK");
				}
			else 
				{
				grlib_SetRedrawCallback (Redraw, NULL);
				if (ret == INTERACTIVE_RET_SE && !CheckParental(1)) continue;
				if (ret == INTERACTIVE_RET_WM && !CheckParental(1)) continue;
				break;
				}
			}
		while (TRUE);
		Video_LoadTheme (0);
		}
		
	MasterInterface (1, 0, TEX_HGL, "Please wait...");
	
	if (ret == INTERACTIVE_RET_SE)
		{
		DirectDolBoot (vars.sePath, "pl", 1);
		return(0);
		}

	if (ret == INTERACTIVE_RET_WM)
		{
		DirectDolBoot (vars.wmPath, "pl", 1);
		return(0);
		}

	if (ret == INTERACTIVE_RET_REBOOT)
		{
		Shutdown ();
		SYS_ResetSystem(SYS_RESTART,0,0);
		return(0);
		}

	if (ret == INTERACTIVE_RET_BOOTMII)
		{
		Shutdown ();
		IOS_ReloadIOS(254);
		return(0);
		}

	if (ret == INTERACTIVE_RET_NEEK2O)
		{
		Shutdown ();
		Neek2oBoot ();
		return(0);
		}

	if (ret == INTERACTIVE_RET_SHUTDOWN || grlibSettings.wiiswitch_poweroff)
		{
		Shutdown ();
		SYS_ResetSystem( SYS_POWEROFF, 0, 0 );
		}
	
	if (ret == INTERACTIVE_RET_WIIMENU || grlibSettings.wiiswitch_reset) // System menu
		{
		BootToSystemMenu ();
		}

	if (ret == INTERACTIVE_RET_HBC) // Return to homebrew channel
		{
		BootToHBC ();
		}

	if (ret == INTERACTIVE_RET_DISC)
		{
		// Tell DML to boot the game from sd card
		*(u32 *)0x80001800 = 0xB002D105;
		DCFlushRange((void *)(0x80001800), 4);
		ICInvalidateRange((void *)(0x80001800), 4);			
		
		*(volatile unsigned int *)0xCC003024 |= 7;
		
		Disc ();
		}

	if (ret == INTERACTIVE_RET_GAMESEL)
		{
		if (vars.neek != NEEK_NONE)
			Disc ();
		else
			RunLoader();
		}

	if (ret == INTERACTIVE_RET_WIILOAD) // boot a channel
		{
		ExtConfigWrite ();
		BootWiiload ();
		}

	if (ret == INTERACTIVE_RET_HBSEL) // boot a channel
		{
		if (DolBootPrepare (&config.run))
			{
			// NOTE: Shutdown() is called inside dolboot
			DolBoot ();
			}
		}
	if (ret == INTERACTIVE_RET_CHANSEL) // boot a channel
		{
		// NOTE: Shutdown() is called inside triiforce
		MasterInterface (1, 0, 2, "");
		BootChannel(&config.run);
		}
		
	// We have an autorun ?
	if (config.autoboot.enabled && config.autoboot.appMode == APPMODE_HBA) // boot a channel
		{
		if (DolBootPrepare (&config.autoboot))
			{
			// NOTE: Shutdown() is called inside dolboot
			DolBoot ();
			}
		}

	if (config.autoboot.enabled && config.autoboot.appMode == APPMODE_CHAN) // boot a channel
		{
		// NOTE: Shutdown() is called inside triiforce
		MasterInterface (1, 0, 2, "");
		BootChannel(&config.autoboot);
		}

	Shutdown ();
	
	// Uh ?
    exit(0);  // Use exit() to exit a program, do not use 'return' from main()
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////

