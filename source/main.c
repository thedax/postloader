#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>

#define MAIN // Define global variabiles

#include "wiiload/wiiload.h"
#include "globals.h"
#include "ios.h"
#include "hbcstub.h"
#include "identify.h"
#include "neek.h"
#include "uneek_fs.h"
#include "mem2.h"
#include "channels.h"
#include "bin2o.h"
#include "devices.h"

extern void __exception_setreload(int t); // In the event of a code dump, app will restart
int Disc (void);
bool plneek_GetNandName (void);

void DumpStub (void)
	{
	FILE *f;
	
	u8 *stub = ((u8 *) 0x80001800);
	
	f = fopen ("sd://stub.bin", "wb");
	if (f)
		{
		fwrite (stub, 1, 0x1800 , f);
		fclose (f);
		}
	}

void BootToSystemMenu(void)
	{
	Shutdown (0);
	
	// Let's try to start system menu... use magic word, as we may have priiloader behind
	*(vu32*)0x8132FFFB = PRIILOADER_TOMENU;
	DCFlushRange((void*)0x8132FFFB,4);
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	}
	
void WiiLoad (int start)
	{
	if (start)
		WiiLoad_Start (vars.tempPath, 5);
	else
		WiiLoad_Stop ();
	}
	
void Subsystems (bool enable)
	{
	if (enable)
		{
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
		}
	}

void Shutdown(bool doNotKillHBC)
	{
	Debug ("Shutdown !");
	
	snd_Stop ();
	cfg_Free (vars.titlestxt);

	Video_Deinit ();
	Subsystems (false);
	
	if (config.usesStub)
		{
		//Debug ("Loading stub");
		StubLoad ();
		}
	else
		{
		// Kill HBC stub
		if (!doNotKillHBC)
			{
			*(u32*)0x80001804 = (u32) 0L;
			*(u32*)0x80001808 = (u32) 0L;
			DCFlushRange((void*)0x80001804,8);
			}
			
		// Also modify it
		Set_Stub (((u64)(1) << 32) | (2));
		}
	
	if (vars.neek != NEEK_NONE) // We are not working under neek
		{
		exit_uneek_fs ();
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
	grlib_printf (320, 440, GRLIB_ALIGNCENTER, 0, "postLoader"VER" (c) 2011 by stfour");
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
		vars.usbtime = 1;
		vars.ios = ios_ReloadIOS (IOS_SNEEK, &vars.ahbprot); // Let's try to reload
		
		init_uneek_fs (ISFS_OPEN_READ|ISFS_OPEN_WRITE);
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
		/*
		if (!vars.ios) // We where not able to patch ahbprot, so reload cios (249...)
			{
			vars.ios = ios_ReloadIOS (IOS_CIOS, &vars.ahbprot);
			}
		*/
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
	
	if (!ret)
		{
		char buff[100];
		char mex[500];
		
		grlib_SetRedrawCallback (Redraw, NULL);
		
		strcpy (buff, "Boot to WiiMenu##0");
		
		if (devices_Get(DEV_SD)) strcat (buff, "|Create on SD and start postLoader##1");
		if (devices_Get(DEV_USB)) strcat (buff, "|Create on USB and start postLoader##2");
		
		sprintf (mex, "Welcome to postLoader"VER"\n\n"
						"No configuration file was found.\n\n"
						"postLoader need a device to keep it's configuration data,\n"
						"so an SD or an USB (FAT/NTFS) device is needed and one must\n"
						"be selected to store postLoader data\n"
						"IOS = %d", vars.ios);
		
		int item = grlib_menu ( mex, buff);
		
		if (item <= 0)
			*vars.defMount = '\0';								
		
		if (grlibSettings.wiiswitch_poweroff)
			{
			Shutdown (0);
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
				grlib_menu ("Configuration file created succesfully", "OK");
				}
			}
		}
		
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
	if (!((grlibSettings.wiiswitch_poweroff || grlibSettings.wiiswitch_reset)) && ret != INTERACTIVE_RET_HOME && vars.interactive)
		{
		Debug ("Showing gui....");
		
		Video_LoadTheme (1);
		CoverCache_Start ();
		snd_Init ();
		WiiLoad (1);
		neek_UID_Read ();
		
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
			else break;
			}
		while (TRUE);
		Video_LoadTheme (0);
		}
	
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

	if (ret == INTERACTIVE_RET_BOOTMII)
		{
		Shutdown (0);
		IOS_ReloadIOS(254);
		return(0);
		}

	if (grlibSettings.wiiswitch_poweroff)
		{
		Shutdown (0);
		SYS_ResetSystem( SYS_POWEROFF, 0, 0 );
		}
	
	if (ret == INTERACTIVE_RET_HOME || grlibSettings.wiiswitch_reset) // System menu
		{
		BootToSystemMenu ();
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
		if (DolBootPrepareWiiload ())
			{
			// NOTE: Shutdown() is called inside dolboot
			DolBoot ();
			}
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

	Shutdown (0);
	
	// Uh ?
    exit(0);  // Use exit() to exit a program, do not use 'return' from main()
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////

