#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <wiiuse/wpad.h>

#include "wiiload/wiiload.h"
#include "globals.h"
#include "ios.h"
#include "hbcstub.h"
#include "identify.h"
#include "neek.h"

// Outside
void *allocate_memory(u32 size);
int AutoUpdate (void);
bool plneek_ShowMenu (void);
void CreatePriiloaderSettings (char *nandpath, u8 * iniBuff, s32 iniSize);

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
	
	for (i = 0; i < nandConfig->NandCnt; i++)
		grlib_menuAddItem (menu, i, (char*)nandConfig->NandInfo[i]);
		
	grlib_menuAddSeparator (menu);
	
	grlib_menuAddItem (menu, -1, "Cancel");	
	int ret = grlib_menu ("Select new neek2o nand", menu);
	
	if (ret == -1) return 0;
	
	// Change nand
	if (ret == nandConfig->NandSel) return 0; // nothing to do!
	
	nandConfig->NandSel = ret;
	neek_WriteNandConfig ();
	Shutdown (0);
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
	
	return 0;
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

/////////////////////////////////////////////////////////////////////////////////////////////////////

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
	grlib_menuAddItem (options, 14,  "Change theme...");
	
	if (vars.neek == NEEK_2o && IsDevValid (DEV_SD) && fsop_FileExist ("sd://neekbooter.dol"))
		grlib_menuAddItem (options, 13,  "Install neekbooter.dol in priiloader...");

	grlib_menuAddItem (options, 1,  "About postLoader"VER);
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

	if (item == 13)
		{
		if (InstallNeekbooter())
			grlib_menu ("Succesfully installed", "OK");
		else
			grlib_menu ("Something gone wrong. Is priiloader installed in nand ?", "OK");
		}

	if (item == 14)
		{
		ThemeSelect();
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

bool InstallNeekbooter (void)
	{
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char fn[64];
	s32 fd;
	s32 ret;
	s32 loaderIniSize = 0;
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
