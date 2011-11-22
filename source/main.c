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

int Disc (void);
int AutoUpdate (void);
bool plneek_ShowMenu (void);
bool plneek_GetNandName (void);

void BootToSystemMenu(void)
	{
	Shutdown (0);
	
	// Let's try to start system menu... use magic word, as we may have priiloader behind
	*(vu32*)0x8132FFFB = CFG_HOME_WII_MENU;
	DCFlushRange((void*)0x8132FFFB,4);
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	}
	
void WiiLoad (int start)
	{
	if (start)
		WiiLoad_Start (vars.tempPath);
	else
		WiiLoad_Stop ();
	}
	
void Subsystems (bool enable)
	{
	if (enable)
		{
		grlib_Controllers (true);
		MountDevices (1);

		WiiLoad (1);
		}
	else
		{
		WiiLoad (0);

		grlib_Controllers (false);
		Fat_Unmount ();
		}
	}

void Shutdown(bool doNotKillHBC)
	{
	Video_Deinit ();
	Subsystems (false);
	
	// Kill HBC stub
	if (!doNotKillHBC)
		{
		*(u32*)0x80001804 = (u32) 0L;
		*(u32*)0x80001808 = (u32) 0L;
		DCFlushRange((void*)0x80001804,8);
		}
		
	// Also modify it
	Set_Stub (((u64)(1) << 32) | (2));
	
	if (vars.neek != NEEK_NONE) // We are not working under neek
		{
		exit_uneek_fs ();
		}
	}

int Initialize(void)
	{
	Video_Init ();
	
	// Prepare the background
	MasterInterface (1, 0, 1, " ");
	MasterInterface (1, 0, 1, " ");
	MasterInterface (1, 0, 1, " ");

	return MountDevices (0);
	}
	
static void Redraw (void)
	{
	Video_DrawBackgroud (0);
	
	grlib_SetFontBMF(fonts[FNTNORM]);
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

	grlib_SetFontBMF(fonts[FNTSMALL]);
	grlib_Text (320, 400, GRLIB_ALIGNCENTER, 0, mex);
	if (p) grlib_Text (320, 420, GRLIB_ALIGNCENTER, 0, p);

	Video_DrawIcon (icon, 320,360);
	if (showCursor) grlib_DrawIRCursor ();
	
	grlib_Render();  // Render the frame buffer to the TV
	
	return 0;
	}
	
void CheckNeek (void)
	{
	u32 boot2version;

#ifdef DOLPHINE	
	vars.neek = 1;
	vars.usbtime = 1;
	return;
#endif

	vars.usbtime = 15;

	ES_GetBoot2Version(&boot2version);
	if (boot2version >= 5) 
		{
		vars.neek = NEEK_USB;
		vars.usbtime = 1;
		vars.ios = ios_ReloadIOS (-1, &vars.ahbprot); // Let's try to reload
		
		init_uneek_fs (ISFS_OPEN_READ|ISFS_OPEN_WRITE);
		}
	}
	
int main(int argc, char **argv) 
	{
	int i;
	int ret;
	time_t t,tout;
	
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
		if (!USE_IOS_DEFAULT)
			vars.ios = ios_ReloadIOS (-1, &vars.ahbprot); // Let's try to reload
		
		if (!vars.ios) // We where not able to patch ahbprot, so reload to standard ios (249...)
			{
			vars.ios = ios_ReloadIOS (IOS_DEFAULT, &vars.ahbprot);
			}
		}
#endif

#ifdef DOLPHINE
	vars.ios = IOS_GetVersion();
#endif

	ret = Initialize();
	
	DebugStart ();
	
	Debug ("-----[PL:"VER"]--------------------------------------------------------------------");
	Debug ("Initialization done !");

	if (vars.neek)
		{
		neek_GetNandConfig ();
		
		if (neek_IsNeek2o())
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
		}

	// This is just for testing... no really needed
	// Identify_SU (0);
	
	/* Prepare random seed */
	srand(time(0)); 
	
	t = time(NULL);
	
	if (!ret)
		{
		char buff[100];
		char mex[500];
		
		grlib_SetRedrawCallback (Redraw, NULL);
		
		strcpy (buff, "Boot to WiiMenu'##0");
		
		if (IsDevValid(DEV_SD)) strcat (buff, "|Create on SD and start postLoader##1");
		if (IsDevValid(DEV_USB)) strcat (buff, "|Create on USB and start postLoader##2");
		
		sprintf (mex, "Welcome to postLoader"VER"\n\n"
						"No configuration file was found.\n\n"
						"postLoader need a device to keep it's configuration data,\n"
						"so an SD or an USB (FAT32) device is needed and one must\n"
						"be selected to store postLoader data\n"
						"IOS = %d", vars.ios);
		
		int item = grlib_menu ( mex, buff);
								
		
		if (item == 0)
			{
			BootToSystemMenu ();
			return (0);
			}
			
		if (item == 1)
			SetDefMount (DEV_SD);
		if (item == 2)
			SetDefMount (DEV_USB);
		
		CheckForPostLoaderFolders ();
		
		if (ConfigWrite())
			{
			grlib_menu ("Configuration file created succesfully", "OK");
			}
		}
		
	Debug ("neek = %d", vars.neek);
	
	Debug ("Autoboot = %d", config.autoboot.enabled);
	if (!config.autoboot.enabled) interactive = 1;

	if (vars.neek != NEEK_NONE && vars.neek != NEEK_2o)
		plneek_GetNandName ();

	if (!interactive)
		{
		if (time(NULL) - t > 5)
			tout = time(NULL) + 1;
		else
			tout = time(NULL) + 3;
		
		while (time(NULL) < tout)
			{
			if (MasterInterface (0, 0, 2, "Press (A) to enter in interactive mode...") == 1) interactive = 1;
			mssleep (10);
			
			if (interactive || (grlibSettings.wiiswitch_poweroff || grlibSettings.wiiswitch_reset)) break;
			}
		}

	// Clean last executed microsneek
	Microsneek (NULL, 0);

	sprintf (vars.tempPath, "%s://ploader/temp", vars.defMount);
	
	ret = INTERACTIVE_RET_NONE;
	
	if (!((grlibSettings.wiiswitch_poweroff || grlibSettings.wiiswitch_reset)) && ret != INTERACTIVE_RET_HOME && interactive)
		{
		Video_LoadTheme (1);
		WiiLoad (1);
		do
			{
			if (config.browseMode == BROWSE_HB) ret = AppBrowser ();
			if (config.browseMode == BROWSE_CH) ret = ChnBrowser ();
			if (config.browseMode == BROWSE_GM) ret = GameBrowser ();
			
			if (ret == INTERACTIVE_RET_TOCHANNELS) 
				config.browseMode = BROWSE_CH;
			else if (ret == INTERACTIVE_RET_TOHOMEBREW)
				config.browseMode = BROWSE_HB;
			else if (ret == INTERACTIVE_RET_TOGAMES)
				config.browseMode = BROWSE_GM;
			else break;
			}
		while (TRUE);
		Video_LoadTheme (0);
		}
	
	ConfigWrite ();
	
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

	if (ret == INTERACTIVE_RET_DISC || ret == INTERACTIVE_RET_GAMESEL) // Disk
		{
		Disc ();
		}

	if (ret == INTERACTIVE_RET_WIILOAD) // boot a channel
		{
		if (DolBootPrepareWiiload ())
			{
			// NOTE: Shutdown() is called inside dolboot
			DolBoot (NULL);
			}
		}

	if (ret == INTERACTIVE_RET_HBSEL) // boot a channel
		{
		if (DolBootPrepare (&config.run))
			{
			// NOTE: Shutdown() is called inside dolboot
			DolBoot (&config.run);
			}
		}
	if (ret == INTERACTIVE_RET_CHANSEL) // boot a channel
		{
		// NOTE: Shutdown() is called inside triiforce
		MasterInterface (1, 0, 2, "");
		tfboot(&config.run);
		}
		
	// We have an autorun ?
	if (config.autoboot.enabled && config.autoboot.appMode == APPMODE_HBA) // boot a channel
		{
		if (DolBootPrepare (&config.autoboot))
			{
			// NOTE: Shutdown() is called inside dolboot
			DolBoot (&config.autoboot);
			}
		}

	if (config.autoboot.enabled && config.autoboot.appMode == APPMODE_CHAN) // boot a channel
		{
		// NOTE: Shutdown() is called inside triiforce
		MasterInterface (1, 0, 2, "");
		tfboot(&config.autoboot);
		}

	// Uh ?
    exit(0);  // Use exit() to exit a program, do not use 'return' from main()
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////

void ShowAboutPLMenu (void)
	{
	char buff[1024];
	
	grlib_SetFontBMF(fonts[FNTNORM]);
	
	strcpy (buff, 	
	"postLoader"VER" by stfour (2011)\n\n"
	"postLoader is intended to be an extension to priiloader autoboot feature.\n"
	"It aims to replace Forwarders and HB Channel (maybe... one day).\n\n"
	"Wii coding learned and inspired by:\n"
	"GRRLIB, TriiForce, Priiloader, CFG Usb Loader, WiiMC, USBLoaderGX...\n"
	"... and sorry if I forgot you ;)\n\n"
	"Official support thread on http://gbatemp.net/\n");

	grlib_menu (buff, "Close");
	}


void ShowAboutMenu (void)
	{
	int item;
	char buff[1024];
	char options[300];
	char path[256];
	
	if (vars.neek == NEEK_NONE)
		strcpy (path, "sd://ploader.sd");
	else
		strcpy (path, "sd://ploader/sdonly.nek");
	
	grlib_SetFontBMF(fonts[FNTNORM]);
	
	strcpy (buff, "Options:\n");
	strcat (buff, "-------------------------\n");
	strcat (buff, "Current autoboot setting:\n");
	if (!config.autoboot.enabled)
		strcat (buff, "* Autoboot is disabled");
	else
		{
		if (config.autoboot.appMode == APPMODE_HBA)
			{
			strcat (buff, "* Homebrew application:\n");
			strcat (buff, "* PATH: ");
			strcat (buff, config.autoboot.path);
			strcat (buff, "\n");
			strcat (buff, "* FILE: ");
			strcat (buff, config.autoboot.filename);
			strcat (buff, "\n");
			strcat (buff, "* ARGS: ");
			strcat (buff, config.autoboot.args);
			}
		if (config.autoboot.appMode == APPMODE_CHAN)
			{
			strcat (buff, "* Channel application:\n");
			strcat (buff, "* NAND: ");
			if (config.autoboot.nand == NAND_REAL)
				strcat (buff, "Real NAND");
			if (config.autoboot.nand == NAND_EMUSD)
				strcat (buff, "Emulated on SD");
			if (config.autoboot.nand == NAND_EMUUSB)
				strcat (buff, "Emulated on USB");
			
			strcat (buff, "\n");

			strcat (buff, "* TITLE: ");
			strcat (buff, config.autoboot.asciiId);
			strcat (buff, "\n");

			strcat (buff, "* NAME: ");
			strcat (buff, config.autoboot.path);
			}
		}
	
	options[0] = '\0';
	
	if (vars.neek == NEEK_2o)
		menu_SwitchNandAddOption (options);
	else
		grlib_menuAddItem (options, 10,  "Change UNEEK nand");
	
	grlib_menuAddSeparator (options);

	if (IsDevValid (DEV_SD))
		{
		if (fsop_FileExist (path))
			grlib_menuAddItem (options, 8,  "Enable boot time USB initialization");
		else
			grlib_menuAddItem (options, 7,  "Disable boot time USB initialization");
		}
	
	if (config.autoboot.enabled)
			grlib_menuAddItem (options, 6,  "Disable autoboot");
	
	grlib_menuAddItem (options, 2,  "Check online for updates");
	grlib_menuAddItem (options, 11,  "Reboot...");
	grlib_menuAddItem (options, 12,  "Clean channel/games configuration files...");

	grlib_menuAddItem (options, 1,  "About postLoader2");
	grlib_menuAddItem (options, 0,  "Close");
	
	item = grlib_menu (buff, options);
	
	if (item == 1)
		{
		ShowAboutPLMenu ();
		}

	if (item == 2)
		{
		AutoUpdate ();
		}

	if (item == 7)
		{
		FILE *f;
		f = fopen (path, "wb");
		if (f) fclose(f);
		}

	if (item == 8)
		{
		unlink (path);
		}

	if (item == 10)
		{
		plneek_ShowMenu ();
		}

	if (item == 11)
		{
		grlib_menu ("Your Wii will now rebooot", "OK");

		Shutdown (0);
		SYS_ResetSystem(SYS_RESTART,0,0);
		}

	if (item == 12)
		{
		CleanTitleConfiguration();
		}

	if (item == MENU_CHANGENEEK2o)
		{
		menu_SwitchNand();
		}

	if (item == 6)
		{
		int item = grlib_menu ("Please confirm:\n\n"
								"You are about to clear autorun application/channel\n"
								"Continue ?\n",
								"OK|Cancel");
		if (item == 0)
			{
			config.autoboot.enabled = 0;
			}
		}

	ConfigWrite ();
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
