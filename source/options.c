#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>

#include "wiiload/wiiload.h"
#include "globals.h"
#include "ios.h"
#include "identify.h"
#include "neek.h"
#include "devices.h"
#include "language.h"

// Outside
void *allocate_memory(u32 size);
int AutoUpdate (void);
bool plneek_ShowMenu (void);
bool CreatePriiloaderSettings (char *nandpath, u8 * iniBuff, s32 iniSize);

// Inside
bool InstallNeekbooter (void);

/*
neek2o nand menu
*/

s32 menu_SwitchNandAddOption (char *menu)
	{
	if (vars.neek == NEEK_2o)
		{
		grlib_menuAddItem (menu, MENU_CHANGENEEK2o, "Change current neek2o nand");
		}
	return 0;
	}

s32 menu_SwitchNand (void)
	{
	int i;
	char menu[1024];
	
	*menu = '\0';
	
	if (!nandConfig)
		{
		grlib_menu (0, "No nands to select", "   OK   ");
		return 0;
		}
	
	for (i = 0; i < nandConfig->NandCnt; i++)
		grlib_menuAddItem (menu, i, (char*)nandConfig->NandInfo[i]);
		
	grlib_menuAddSeparator (menu);
	
	grlib_menuAddItem (menu, -1, "Cancel");	
	int ret = grlib_menu (0, "Select new neek2o nand", menu);
	
	if (ret == -1) return 0;
	
	// Change nand
	if (ret == nandConfig->NandSel) return 0; // nothing to do!
	
	nandConfig->NandSel = ret;
	neek_WriteNandConfig ();
	Shutdown ();
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	
	return 0;
	}
	
/////////////////////////////////////////////////////////////////////////////////////////////////////
	
void ShowCreditsMenu (void)
	{
	char buff[1024];
	
	Video_SetFont(TTFNORM);
	
	sprintf (buff, 	
	"credits\n"
	"\n"
	"coding:\n"
	"WiiFlow, NeoGamma,GRRLIB,TriiForce,Priiloader,CFG Loader,WiiMC,USBLoaderGX\nNintendont\n"
	"people:\n"
	"FIX94(wiiflow/dmlbooter),JoostinOnline(S.E.),obcd (neek2o/uneekfs),XFlak (modmii)\n"
	"and all active testers:\n"
	"Wever,daxtsu,ZFA,AbdallahTerro... and sorry if I forgot you ;)\n\n"
	"Official support thread on http://gbatemp.net/\n");
	
	grlib_menu (0, buff, GLS("Close"));
	}

void ShowAboutPLMenu (void)
	{
	char buff[1024];
	
	char autoboot[1024];
	
	if (!config.autoboot.enabled)
		sprintf (autoboot, "Autoboot: disabled");
	else
		{
		if (config.autoboot.appMode == APPMODE_HBA)
			{
			sprintf (autoboot, "Autoboot: HB %s%s %s", config.autoboot.path, config.autoboot.filename, config.autoboot.args);
			}
		if (config.autoboot.appMode == APPMODE_CHAN)
			{
			char nand[64];
			
			if (config.autoboot.nand == NAND_REAL)
				strcat (nand, "");
			if (config.autoboot.nand == NAND_EMUSD)
				strcat (nand, "EmuSD");
			if (config.autoboot.nand == NAND_EMUUSB)
				strcat (nand, "EmuUSB");

			sprintf (autoboot, "Autoboot: CH %s:%s:%s", nand, config.autoboot.asciiId, config.autoboot.path);
			}
		}

	Video_SetFont(TTFNORM);
	
	sprintf (buff, 	
	"postLoader"VER" by stfour (2011-12)\n\n"
	"%s\n"
	"WII IP: %s\n\n"
	"Official support thread on http://gbatemp.net/\n", autoboot, wiiload.ip);

	int ret;
	
	do
		{
		ret = grlib_menu (0, buff, " %s ##0~ %s ##1", GLS("Close"), "CREDITS");
		if (ret == 1) ShowCreditsMenu();
		}
	while (ret);
	}

/////////////////////////////////////////////////////////////////////////////////////////////////////
static void cb_filecopy (void)
	{
	static u32 mstout = 0;
	u32 ms = ticks_to_millisecs(gettime());

	if (mstout < ms)
		{
		u32 mb = (u32)((fsop.multy.bytes/1000)/1000);
		u32 sizemb = (u32)((fsop.multy.size/1000)/1000);
		u32 elapsed = time(NULL) - fsop.multy.startms;
		u32 perc = (mb * 100)/sizemb;
		
		Video_WaitPanel (TEX_HGL, 0, GLS("pwcopying"), perc, mb, sizemb, (u32)(fsop.multy.bytes/elapsed));
		
		mstout = ms + 250;
		
		if (grlib_GetUserInput() & WPAD_BUTTON_B)
			{
			int ret = grlib_menu (0, GLS("interruptcopy"), GLS("YesNo"));
			if (ret == 0) fsop.breakop = 1;
			}
		snd_Mp3Go ();
		}
	}

static void cb_remove1(void)
	{
	static time_t lastt = 0;
	time_t t = time(NULL);
	
	if (t - lastt >= 1)
		{
		Video_WaitPanel (TEX_HGL, 0, GLS("clrusbdata"));
		lastt = t;
		}
	}
	
static void cb_remove2(void)
	{
	static time_t lastt = 0;
	time_t t = time(NULL);
	
	if (t - lastt >= 1)
		{
		Video_WaitPanel (TEX_HGL, 0, GLS("clrsddata"));
		lastt = t;
		}
	}

static void cb_remove3(void)
	{
	static time_t lastt = 0;
	time_t t = time(NULL);
	
	if (t - lastt >= 1)
		{
		Video_WaitPanel (TEX_HGL, 0, "Clearing TEX cache");
		lastt = t;
		}
	}

void ShowAdvancedOptions (void)
	{
	int item;
	char buff[1024];
	char options[300];
	
	Video_SetFont(TTFNORM);
	
	do
		{
		strcpy (buff, GLS("advopt1"));
		strcat (buff, GLS("advopt2"));
		
		*options = '\0';
		
		if (vars.neek == NEEK_NONE)
			grlib_menuAddCustomCheckItem (options, 1, extConfig.disableUSB, GLS("Yes|No"), GLS("srcusb"));
		else
			grlib_menuAddCustomCheckItem (options, 1, extConfig.disableUSBneek, GLS("Yes|No"), GLS("srcusb"));

		if (vars.neek == NEEK_NONE)
			{
			grlib_menuAddCustomCheckItem (options, 2, extConfig.use249, GLS("Yes|No"), GLS("useciosx"));
			}

		if (vars.neek == NEEK_NONE)
			{
			grlib_menuAddCustomCheckItem (options, 3, config.runHBwithForwarder, GLS("Yes|No"), GLS("usepl3ch"));
			}

		grlib_menuAddCustomCheckItem (options, 6, config.enableTexCache, GLS("Yes|No"), "Enable TEX cache");
		Debug ("config.enableTexCache %d", config.enableTexCache);

		grlib_menuAddCustomCheckItem (options, 5, config.usesGestures, GLS("Yes|No"), GLS("usegest"));
			
		grlib_menuAddItem (options, 4,  GLS("rstrbt"));
		
		grlib_menuAddSeparator (options);
		grlib_menuAddItem (options, -1,  GLS("close"));
		
		Video_SetFontMenu(TTFSMALL);
		item = grlib_menu (0, buff, options);
		Video_SetFontMenu(TTFNORM);
		
		if (item == 1)
			{
			if (vars.neek == NEEK_NONE)
				extConfig.disableUSB = !extConfig.disableUSB;
			else
				extConfig.disableUSBneek = !extConfig.disableUSBneek;
			}
			
		if (item == 2)
			{
			extConfig.use249 = !extConfig.use249;
			}
			
		if (item == 6)
			{
			config.enableTexCache = !config.enableTexCache;
			CoverCache_Flush ();
			
			if (config.enableTexCache)
				{
				char path[PATHMAX];
				sprintf (path, "%s://ploader/tex",vars.defMount);
				if (!fsop_DirExist (path))
					{
					if (!fsop_MakeFolder (path))
						{
						grlib_menu (50, "Error:\nCannot create /ploader/tex folder", "OK");
						config.enableTexCache = 0;
						}
					}
				}
			else
				{
				Video_WaitIcon (TEX_HGL);
				
				char path[PATHMAX];
				sprintf (path, "%s://ploader/tex",vars.defMount);
				fsop_KillFolderTree (path, cb_remove3);
				}
			}

		if (item == 3)
			{
			config.runHBwithForwarder = !config.runHBwithForwarder;
			}

		if (item == 4)
			{
			int ret = grlib_menu (0, GLS("rstrbtcanc1"), GLS("rstrbtcanc2"));
			if (ret < 0) continue;
			
			vars.saveExtendedConf = 1;
			ConfigWrite ();
			ExtConfigWrite ();

			if (ret == 0)
				{
				ReloadPostloader ();
				}
			if (ret == 1)
				{
				Shutdown ();
				SYS_ResetSystem(SYS_RESTART,0,0);
				}
			}

		if (item == 5)
			{
			config.usesGestures = !config.usesGestures;
			}
		}
	while (item != -1);
	
	// Update grlib setting...
	grlibSettings.usesGestures = config.usesGestures;
	vars.saveExtendedConf = 1;
	ExtConfigWrite ();
	}

static int n2oini_Manage(int mode) // 0 = check, 1 = write, 2 = remove
	{
	s32 fd;
	int ret = 0;
	
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	ISFS_Initialize ();

	sprintf (path, "/title/00000001/00000002/data/n2oboot.ini");
	if (mode == 0)
		{
		fd = ISFS_Open(path, ISFS_OPEN_READ);
		if (fd > 0)
			{
			ISFS_Close(fd);
			ret = 1;
			goto exit;
			}
		}

	if (mode == 1)
		{
		char buff[32] ATTRIBUTE_ALIGN(32);
		
		memset (buff, 0, sizeof(buff));
		
		ISFS_CreateFile (path,0,3,3,3);
		fd = ISFS_Open (path, ISFS_OPEN_RW);
		if (fd > 0)
			{
			ret = ISFS_Write (fd, buff, sizeof(buff));
			ISFS_Close(fd);
			ret = 1;
			goto exit;
			}
		}

	if (mode == 2)
		{
		ret = ISFS_Delete (path);
		}

exit:
	ISFS_Deinitialize ();
	return ret;
	}

void ShowNeekOptions (void)
	{
	int item;
	int n2oini;
	char buff[1024];
	char options[300];
	
	Video_SetFont(TTFNORM);
	
	do
		{
		strcpy (buff, GLS("n2oopt"));
		strcat (buff, GLS("n2onotice"));
		
		*options = '\0';
		
		n2oini = n2oini_Manage (0);
		
		if (n2oini)
			grlib_menuAddItem (options, 1,  GLS("n2obooterstartpl"));
		else
			grlib_menuAddItem (options, 1,  GLS("n2obooternostartpl"));

		grlib_menuAddSeparator (options);
		grlib_menuAddItem (options, -1,  GLS("Close"));
		
		Video_SetFontMenu(TTFSMALL);
		item = grlib_menu (0, buff, options);
		Video_SetFontMenu(TTFNORM);
		
		if (item == 1 && n2oini)
			{
			n2oini_Manage (2); // remove
			}
		
		if (item == 1 && !n2oini)
			{
			n2oini_Manage (1); // create
			}

		}
	while (item != -1);
	}

void SetVolume (void)
	{
	int item;
	char buff[512];
	
	Video_SetFont(TTFSMALL);
	
	do
		{
		*buff = '\0';
		grlib_menuAddCheckItem (buff, 1, config.volume == 1, "1 (low)");
		grlib_menuAddCheckItem (buff, 2, config.volume == 2, "2");
		grlib_menuAddCheckItem (buff, 3, config.volume == 3, "3 (medium)");
		grlib_menuAddCheckItem (buff, 4, config.volume == 4, "4");
		grlib_menuAddCheckItem (buff, 5, config.volume == 5, "5 (hi)");
		grlib_menuAddSeparator (buff);
		grlib_menuAddItem (buff, -1, "Close");

		item = grlib_menu (0, "Select volume", buff);
		
		if (item > 0)
			{
			config.volume = item;
			snd_Volume();
			}
		}
	while (item > 0);
	
	Video_SetFont(TTFNORM);
	}

void ShowAboutMenu (void)
	{
	if (!CheckParental(2)) return;

	int item;
	char options[300];
	
	Video_SetFont(TTFNORM);
	
	do
		{
		*options = '\0';
		
		grlib_menuAddItem (options, 14,  "Change theme...");

		if (vars.neek == NEEK_2o)
			menu_SwitchNandAddOption (options);
		else
			grlib_menuAddItem (options, 10,  "Change UNEEK NAND");

		if (config.autoboot.enabled)
			grlib_menuAddItem (options, 6,  "Disable autoboot");
		
		grlib_menuAddItem (options, 2,  "Online update...");
		
		grlib_menuAddItem (options, 7,  "Advanced options...");
		
		if (vars.neek != NEEK_NONE)
			grlib_menuAddItem (options, 8,  "NEEK options...");
			
		if (vars.neek == NEEK_2o && devices_Get (DEV_SD) && fsop_FileExist ("sd://neekbooter.dol"))
			grlib_menuAddItem (options, 13,  "Install neekbooter.dol in priiloader...");
			
		if (devices_Get (DEV_SD) && devices_Get (DEV_USB) && strcmp (vars.defMount, devices_Get(DEV_SD)) == 0)
			grlib_menuAddItem (options, 15,  "Move postLoader cfg folder to USB");
			
		grlib_menuAddItem (options, 16,  "Set parental control (password)");
		grlib_menuAddItem (options, 17,  "Set volume (%d)", config.volume);

		item = grlib_menu (0, "Options:", options);
		
		if (item == 2)
			{
			AutoUpdate ();
			}

		if (item == 7)
			{
			ShowAdvancedOptions ();
			}

		if (item == 8)
			{
			ShowNeekOptions ();
			}

		if (item == 10)
			{
			plneek_ShowMenu ();
			}

		if (item == 13)
			{
			if (InstallNeekbooter())
				grlib_menu (0, "Succesfully installed", "OK");
			else
				grlib_menu (0, "Something gone wrong. Is priiloader installed in nand ?", "OK");
			}

		if (item == 14)
			{
			ThemeSelect();
			}

		if (item == 15)
			{
			fsop_KillFolderTree ("usb://ploader", cb_remove1);
			fsop_CopyFolder ("sd://ploader", "usb://ploader", cb_filecopy);
			
			if (!fsop.breakop)
				{
				grlib_menu (0, "Your Wii will now rebooot", "  OK  ");

				fsop_KillFolderTree ("sd://ploader", cb_remove2);
				Shutdown ();
				SYS_ResetSystem(SYS_RESTART,0,0);
				item = -1;
				}
			}

		if (item == 16)
			{
			grlib_Keyboard ("Change password", config.pwd, PWDMAXLEN);
			}
			
		if (item == 17)
			{
			SetVolume ();
			}
			
		if (item == MENU_CHANGENEEK2o)
			{
			menu_SwitchNand();
			}

		if (item == 6)
			{
			int item = grlib_menu (0,
									"Please confirm:\n\n"
									"You are about to clear autorun application/channel\n"
									"Continue ?\n",
									"OK|Cancel");
			if (item == 0)
				{
				config.autoboot.enabled = 0;
				}
			}
		}
	while (item != -1);
	
	ConfigWrite ();
	}

bool InstallNeekbooter (void)	// if nandpath is null, neekbooter is installed in isfs
	{
	char fn[64];
	s32 fd;
	s32 ret;
	size_t loaderIniSize = 0;
	u8 * loaderIniBuff = NULL;
	char *buff = NULL;
	
	// Load neekbooter.dol in memory
	sprintf (fn, "%s://neekbooter.dol", vars.defMount);
	FILE *f = fopen(fn, "rb");
	Debug ("InstallNeekbooter: fopen %x", f);
	if (!f) return false;
	fseek( f, 0, SEEK_END);
	long buffsize = ftell(f);
	fseek( f, 0, SEEK_SET);
	buff = allocate_memory (buffsize);
	fread(buff, 1, buffsize, f );
	fclose (f);
	
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	ISFS_Initialize();
		
	// Read the loader.ini configuration file
	sprintf (path, "/title/00000001/00000002/data/loader.ini");

	// Get priiloader loader.ini files contents
	fd = ISFS_Open (path, ISFS_OPEN_READ);
	if (fd < 0)
		{
		free (buff);
		return false;
		}
		
	loaderIniSize = ISFS_Seek(fd, 0, 2);
	
	if (loaderIniSize)
		{
		loaderIniBuff = allocate_memory (loaderIniSize);
		ISFS_Seek(fd, 0, 0);
		ISFS_Read(fd, loaderIniBuff, loaderIniSize);
		}
	
	ISFS_Close (fd);
	
	if (!loaderIniSize)
		{
		free (buff);
		return false;
		}

	// Store neekbooter.dol as main.bin
	sprintf (path, "/title/00000001/00000002/data/main.bin");
	ISFS_Delete (path);
	ISFS_CreateFile (path,0,3,3,3);
	fd = ISFS_Open (path, ISFS_OPEN_RW);
	ret = ISFS_Write (fd, buff, buffsize);
	Debug ("InstallNeekbooter: ISFS_Write (%d %d)", ret, buffsize);
	ret = ISFS_Close (fd);
	
	// Change priiloader configuration to autoboot installed file
	CreatePriiloaderSettings ("", loaderIniBuff, loaderIniSize);
	free (loaderIniBuff);

	ISFS_Deinitialize ();
	
	return true;
	}

int ShowExitMenu (void)
	{
	int item;
	char options[300];
	
	*options = '\0';
	
	grlib_menuAddItem (options, 3,  "Shutdown my WII");
	if (vars.neek != NEEK_NONE)
		grlib_menuAddItem (options, 5,  "Return to real WII");
	grlib_menuAddSeparator (options);
	grlib_menuAddItem (options, 1,  "Exit to WII Menu");
	if (CheckParental(0))		
		grlib_menuAddItem (options, 2,  "Exit to Homebrew Channel");
	if (CheckParental(0))		
		grlib_menuAddItem (options, 4,  "Restart postLoader");
	
	item = grlib_menu (0, "Exit: Select an option", options);
	
	if (item == 1) return INTERACTIVE_RET_WIIMENU;
	if (item == 2) return INTERACTIVE_RET_HBC;
	if (item == 3) return INTERACTIVE_RET_SHUTDOWN;
	if (item == 4) ReloadPostloader();
	if (item == 5) return INTERACTIVE_RET_REBOOT;
	
	return -1;
	}

int ShowBootmiiMenu (void)
	{
	int item;
	char options[300];
	
	*options = '\0';
	
	grlib_menuAddItem (options, 1,  "Start NEEK2O (Direct mode)");
	grlib_menuAddSeparator (options);
	grlib_menuAddItem (options, 2,  "Start NEEK/BOOTMII (IOS254 Method)");
	
	item = grlib_menu (0, "NEEK/BOOTMII: Select an option", options);
	
	if (item == 1) return INTERACTIVE_RET_NEEK2O;
	if (item == 2) return INTERACTIVE_RET_BOOTMII;
	
	return -1;
	}

int CheckParental (int mode) // 0 silent, 1 message, 2 ask for password
	{
	static bool unlocked = false;
	char pwd[PWDMAXLEN+1] = {0};
	
	//Debug ("pass = %s (%d)", config.pwd, strlen(config.pwd));

#ifdef DOLPHINE
	return 1;
#endif
	
	
	if (unlocked || strlen(config.pwd) == 0) return 1;
	if (mode == 0) return 0;
	
	Video_SetFont(TTFNORM);	
	
	if (mode == 1)
		{
		grlib_menu (0, "Parental Control is active.\nPlease disable it pressing 'Config' in the bottom bar", "   OK   ");
		return 0;
		}
	
	grlib_Keyboard ("Enter password", pwd, PWDMAXLEN);
	
	if (strcmp (config.pwd, pwd) == 0)
		{
		grlib_menu (0, "postLoader advanced options are now unlocked", "   OK   ");
		unlocked = true;
		return 1;
		}
		
	if (strlen (pwd)) grlib_menu (0, "Wrong password !", "   OK   ");
	return 0;
	}