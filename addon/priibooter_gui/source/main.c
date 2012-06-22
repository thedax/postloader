#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gccore.h>
#include <dirent.h>
#include <wiiuse/wpad.h>
#include <sys/unistd.h>
#include "grlib/grlib.h"
#include "common.h"
#include "debug.h"
#include "fsop.h"
#include "cfg.h"
#include "devices.h"
#include "ios.h"
#include "../build/bin2o.h"

#define USBTOUT 15

#define FNTNORM 0
#define FNTSMALL 1

#define VER "2.7"

#define BASEPATH "usb://nands"
#define PRII_WII_MENU 0x50756E65

#define POSTLOADER_SDAPP "sd://apps/postloader/boot.dol"
#define POSTLOADER_USBAPP "usb://apps/postloader/boot.dol"

#define TITLE_ID(x,y) (((u64)(x) << 32) | (y)) // Needed for launching channels

char dev[64];
char cfg[64];

#define NANDSUBS 10
static char nandsubs[NANDSUBS][32] = 
	{
	"IMPORT",
	"META",
	"SHARED1",
	"SHARED2",
	"SYS",
	"TICKET",
	"TITLE",
	"TMP",
	"WFS",
	"SNEEK"
	};

extern const u8 background_png[];
extern const u8 fnorm_bmf[];
extern const u8 fsmall_bmf[];
extern const u8 wiicursor_png[];

int bkgFromFile = 0;
GRRLIB_texImg *bkg;
GRRLIB_texImg *cursor;
GRRLIB_bytemapFont *fonts[2];

char mex1[256];
char mex2[256];

static int keypressed = 0;
static int alphaback = 0;
static int themeLoaded = 0;
static int errors = 0;
static int doNotRender = 0;
static int showHddIcon = 0;
static GRRLIB_texImg *texHDD;

static int pl_sd = 0;
static int pl_usb = 0;

s_plneek pln; //pln=plneek
static char *nandSource;

bool LoadExecFile (char *path, char *args);
bool BootExecFile (void);

// These two functions are from settings.c
int Settings (void);
void EmuExitPrompt(void);
bool IsNandFolder (char *path);

extern void __exception_setreload(int t); // In the event of a code dump, app will exit after 3 seconds

bool Neek2oLoadKernel (void);
bool Neek2oBoot (void);

static void Redraw (void) // Refresh screen
	{
	static int hdd_angle = 0;

	int fr;
	
	fr = grlibSettings.fontBMF_reverse;
	if (!bkgFromFile) grlibSettings.fontBMF_reverse = 0;
	
	if (alphaback < 0) alphaback = 0;
	if (alphaback > 255) alphaback = 255;
	
	GRRLIB_DrawImg(0, 0, bkg, 0, 1, 1, RGBA(255, 255, 255, alphaback)); // Display the background image
	
	grlib_SetFontBMF (fonts[FNTSMALL]);
	if (strlen(mex1))
		grlib_printf (320, 400, GRLIB_ALIGNCENTER, 0, mex1);

	grlib_SetFontBMF (fonts[FNTSMALL]);
	if (strlen(mex2))
		grlib_printf (320, 420, GRLIB_ALIGNCENTER, 0, mex2);
		
	if (keypressed == 0)
		{
		char buff[128];
		
		if (pln.bootMode == PLN_BOOT_REAL)
			strcpy (buff, "You are in REAL NAND mode");
		if (pln.bootMode == PLN_BOOT_NEEK)
			strcpy (buff, "You are in UNEEK mode");
		if (pln.bootMode == PLN_BOOT_SM)
			strcpy (buff, "You are in System Menu' mode");
		if (pln.bootMode == PLN_BOOT_HBC)
			strcpy (buff, "You are in HomeBrewChannel mode");
		if (pln.bootMode == PLN_BOOT_NEEK2O)
			strcpy (buff, "You are in neek2o mode");
			
		strcat (buff, ": Press any key to change");
			
		grlib_printf (320, 440, GRLIB_ALIGNCENTER, 0, buff);
		}
	else
		grlib_printf (320, 440, GRLIB_ALIGNCENTER, 0, "(Key pressed: Boot menu will be displayed)");

	if (showHddIcon)
		{
		static int fade = 0;
		if (fade < 255) fade += 5;
		hdd_angle++; 
		if (hdd_angle >= 360) hdd_angle = 0;
		GRRLIB_DrawImg( 320 - 18, 360 - 18, texHDD, hdd_angle, 1.0, 1.0, RGBA(255, 255, 255, fade) ); 
		}
	
	if (!doNotRender) 
		{
		grlib_Render();
		if (grlib_GetUserInput())
			{
			keypressed = 1;
			}
			
		if (grlibSettings.wiiswitch_reset)
			{
			keypressed = 1;
			}
		}
	grlibSettings.fontBMF_reverse = fr;
	}
	
void printd(const char *text, ...)
	{
	char Buffer[1024];
	va_list args;

	va_start(args, text);
	vsprintf(Buffer, text, args);

	va_end(args);
	
	strcpy (mex2, Buffer);
	Redraw();
	
	if (strstr (Buffer, "Error") || strstr (Buffer, "Warning"))
		sleep (3);
	else
		sleep (0);
	}
	
void fadeIn (void) 
	{
	//The following loop will take little less than 3 second to complete
	
	for (alphaback = 0; alphaback < 255; alphaback += 1) // Fade background image out to black screen
		{
		Redraw ();
		usleep (10000); // 10msec
		}
	} // fadeOut

void fadeOut (void) 
	{
	for (alphaback = 255; alphaback > 0; alphaback -= 5) // Fade background image out to black screen
		{
		Redraw ();
		}
	} // fadeOut

int Initialize(void)
	{
	grlib_Init();

	bkg = GRRLIB_LoadTexturePNG (background_png); // Background image
	cursor = GRRLIB_LoadTexturePNG (wiicursor_png); // Cursor/pointer
	
	texHDD = GRRLIB_LoadTexturePNG (hdd_png);
	GRRLIB_SetHandle (texHDD, 18, 18);
	
	fonts[FNTNORM] = GRRLIB_LoadBMF (fnorm_bmf);
	fonts[FNTSMALL] = GRRLIB_LoadBMF (fsmall_bmf);

	grlibSettings.fontNormBMF = fonts[FNTNORM];
	grlibSettings.fontSmallBMF = fonts[FNTSMALL];
	
	cursor->offsetx = 10;
	cursor->offsety = 3;
	
	grlib_SetWiimotePointer (cursor, 0); // Set the cursor to start in the top left corner

	return 1;
	} // Initialize
	
int ScanForTheme (int dev)
	{
	char mnt[32];
	char path[300];
	char *cfg;
	
	if (dev == DEV_SD) strcpy(mnt, "sd");
	if (dev == DEV_USB) strcpy(mnt, "usb");
	
	// background
	sprintf (path, "%s://ploader/priibooter.png", mnt);
	if (fsop_FileExist (path))
		{
		if (bkg != NULL) GRRLIB_FreeTexture (bkg);
		bkg = GRRLIB_LoadTextureFromFile (path);
		bkgFromFile = 1;
		}
	
	sprintf (path, "%s://ploader/theme/button.png", mnt);
	grlibSettings.theme.texButton = GRRLIB_LoadTextureFromFile (path);
	
	sprintf (path, "%s://ploader/theme/button_sel.png", mnt);
	grlibSettings.theme.texButtonSel = GRRLIB_LoadTextureFromFile (path);
	
	sprintf (path, "%s://ploader/theme/window.png", mnt);
	grlibSettings.theme.texWindow = GRRLIB_LoadTextureFromFile (path);
	
	sprintf (path, "%s://ploader/theme/windowbk.png", mnt);
	grlibSettings.theme.texWindowBk = GRRLIB_LoadTextureFromFile (path);
	
	sprintf (path, "%s://ploader/theme/theme.cfg", mnt);
	cfg = (char*)fsop_ReadFile (path, 0, NULL);

	if (cfg)
		{
		cfg_GetInt (cfg, "grlibSettings.theme.windowMagX", &grlibSettings.theme.windowMagX);
		cfg_GetInt (cfg, "grlibSettings.theme.windowMagY", &grlibSettings.theme.windowMagY);
		cfg_GetInt (cfg, "grlibSettings.theme.buttonMagX", &grlibSettings.theme.buttonMagX);
		cfg_GetInt (cfg, "grlibSettings.theme.buttonMagY", &grlibSettings.theme.buttonMagY);
		
		cfg_GetInt (cfg, "grlibSettings.theme.buttonsTextOffsetY", &grlibSettings.theme.buttonsTextOffsetY);
		cfg_GetInt (cfg, "grlibSettings.fontBMF_reverse", &grlibSettings.fontBMF_reverse);

		grlibSettings.theme.enabled = true;
		
		themeLoaded = 1;
		
		free (cfg);
		}
	return 0;
	}

//=============================================================================================================================

/*
this function will check if a folder "may" contain a valid nand
*/	
bool IsNandFolder (char *path)
	{
	char npath[300];
	
	sprintf (npath, "%s/TITLE", path);
	if (!fsop_DirExist(npath)) return FALSE;

	sprintf (npath, "%s/TICKET", path);
	if (!fsop_DirExist(npath)) return FALSE;

	sprintf (npath, "%s/SHARED1", path);
	if (!fsop_DirExist(npath)) return FALSE;

	sprintf (npath, "%s/SYS", path);
	if (!fsop_DirExist(npath)) return FALSE;

	return TRUE;
	}

/*
CountObjectsInFolder will count object in a foder. "." and ".." are skipped. 
It return -1 if the passed folder doesn't exist or is a file
*/
int CountObjectsInFolder(char *path)
	{
	int isFolder = 0;
	DIR *pdir;
	struct dirent *pent;
	int count = 0;

	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Let's check if we have really a folder
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			isFolder = 1;
		else
			{
			count ++;
			}

		//printd ("    > %s, %d, pent->d_name, count);
		}
	closedir(pdir);
	if (!isFolder) return -1;

	return count;
	}

/*
this function will retrive all available nand foder in usb://nands
*/	
void GetNandsFolders (void)
	{
	DIR *pdir;
	struct dirent *pent;
	
	char path[256];

	// As there is a nand, we must found the empty folder in usb://nands that was containing it...
	//printd ("Scanning for nand folders...");
	
	pln.nandsCnt = 0;

	strcpy (path, BASEPATH);
	
	pdir=opendir(path);
	while ((pent=readdir(pdir)) != NULL) 
		{
		sprintf (path, BASEPATH"/%s", pent->d_name);
		
		// Let's check if we have really a folder
		if (strcmp (pent->d_name, ".") != 0 &&
			strcmp (pent->d_name, "..") != 0 &&
			fsop_DirExist (path))
			{
			strcpy (pln.nands[pln.nandsCnt].path, path);
			pln.nands[pln.nandsCnt].count = CountObjectsInFolder (pln.nands[pln.nandsCnt].path);
			
			if (pln.nands[pln.nandsCnt].count == 0 || IsNandFolder (pln.nands[pln.nandsCnt].path))
				{
				pln.nandsCnt++; 	// Added
				}
			}
		}
	closedir(pdir);
	}
	
/*
there is a nand in the root of the drive (usb). We need to copy back. We absolutely must not loose it's content
*/	
void StoreCurrentNand (void)
	{
	int i;
	int foundIt;
	char target[256];
	
	if (!IsNandFolder ("usb:/")) return;
	
	// As there is a nand, we must found the empty folder in usb://nands that was containing it...
	printd ("Backing up current nand folder");
	
	// search and empty nand folder
	foundIt = 0;
	for (i = 0; i < pln.nandsCnt; i++)
		if (pln.nands[i].count == 0)
			{
			foundIt = 1;
			break;
			}
				
	// if we not found an empty folder, user (or I LOL) should have made some erros... go to create empty one
	if (foundIt)
		strcpy (target, pln.nands[i].path);
	else
		{
		sprintf (target, BASEPATH"/backup%u", (unsigned int)time(0));
		mkdir (target, S_IREAD | S_IWRITE);

		// Add the new folder to struct
		sprintf (pln.nands[pln.nandsCnt].path, "%s", target);
		pln.nands[pln.nandsCnt].count = 0;
		pln.nandsCnt++;
		}
		
	for (i = 0; i < NANDSUBS; i++)
		{
		char path[256];
		char source[256];
		
		sprintf (source, "usb://%s", nandsubs[i]);
		sprintf (path, "%s/%s", target, nandsubs[i]);
		
		if (fsop_DirExist (source))
			{
			if (rename (source, path) == 0)
				printd (".");
			else
				{
				printd ("Error on mooving %s to %s", nandsubs[i], path);
				sleep (5);
				errors++;
				}
			}
		}
	}

void MoveNewNand (void)
	{
	int i;
	
	// As there is a nand, we must found the empty folder in usb://nands that was containing it...
	printd ("Moving selected nand");
	
	// search and empty nand folder
	for (i = 0; i < NANDSUBS; i++)
		{
		char path[256];
		char source[256];
		
		sprintf (source, "%s/%s", nandSource, nandsubs[i]);
		sprintf (path, "usb://%s", nandsubs[i]);
		
		if (fsop_DirExist (source))
			{
			if (rename (source, path) == 0)
				printd (".");
			else
				{
				printd ("Error on mooving %s to %s", nandsubs[i], path);
				errors++;
				}
			}
		}
	}
	
/*
As pl have no accesso to usb, we need to store the available nands to sd
*/
bool SavePLN (char *path)
	{
	FILE *f;

	//printd ("Storing nand folder informations");

	f = fopen (path, "wb");
	if (!f) return FALSE;
	
	fwrite (&pln, sizeof(s_plneek), 1, f);
	fclose (f);
	
	return TRUE;
	}

bool LoadPLN (char *path)
	{
	FILE *f;

	f = fopen (path, "rb");
	if (!f) 
		{
		pln.bootMode = PLN_BOOT_REAL;
		*pln.name = '\0';
		return FALSE;
		}
	
	fread (&pln, sizeof(s_plneek), 1, f);
	fclose (f);
	
	return TRUE;
	}
	
void BootToMenu(void)
	{
	// Let's try to start system menu... use magic word, as we may have priiloader behind
	*(vu32*)0x8132FFFB = 0x50756E65;
	DCFlushRange((void*)0x8132FFFB,4);
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0); 
	}

//=============================================================================================================================

int ChooseNewMode (void)
	{
	int ret;
	char menu[1024];
	
	doNotRender = 1;
	
	do
		{
		menu[0] = '\0';
		grlib_menuAddCheckItem (menu, 100, (pln.bootMode == PLN_BOOT_REAL), "postLoader");
		grlib_menuAddCheckItem (menu, 101, (pln.bootMode == PLN_BOOT_NEEK), "BOOTMII");
		grlib_menuAddCheckItem (menu, 102, (pln.bootMode == PLN_BOOT_SM), "WII System menu");
		grlib_menuAddCheckItem (menu, 103, (pln.bootMode == PLN_BOOT_HBC), "Homebrew Channel");
		grlib_menuAddCheckItem (menu, 104, (pln.bootMode == PLN_BOOT_NEEK2O), "neek2o");
		grlib_menuAddSeparator (menu);
		grlib_menuAddItem (menu, 1, "Start selected item");
		grlib_menuAddItem (menu, 2, "Save and start selected item");
		
		ret = grlib_menu ("Please select your option", menu);
		
		if (ret == 100) pln.bootMode = PLN_BOOT_REAL;
		if (ret == 101) pln.bootMode = PLN_BOOT_NEEK;
		if (ret == 102) pln.bootMode = PLN_BOOT_SM;
		if (ret == 103) pln.bootMode = PLN_BOOT_HBC;
		if (ret == 104) pln.bootMode = PLN_BOOT_NEEK2O;
		}
	while (ret > 2);
	
	doNotRender = 0;
	
	return ret;
	}

void Boot (void)
	{
	if (keypressed)
		{
		grlibSettings.wiiswitch_reset = 0;
		if (ChooseNewMode () == 2) SavePLN (cfg);
		}
		
	if (grlibSettings.wiiswitch_reset)
		pln.bootMode = PLN_BOOT_SM;
	
	fadeOut(); // Fade out in an orgy of awesomeness

	WPAD_Shutdown();
	PAD_Reset(0xf0000000);

	if (pln.bootMode == PLN_BOOT_NEEK)
		{
		grlib_Exit ();
		devices_Unmount ();
		IOS_ReloadIOS (254); 
		}

	if (pln.bootMode == PLN_BOOT_REAL)
		{
		bool found = FALSE;
		
		if (pl_sd && !found) found = LoadExecFile (POSTLOADER_SDAPP, "priibooter");
		if (pl_usb && !found) found = LoadExecFile (POSTLOADER_USBAPP, "priibooter");

		if (!found) BootToMenu ();
				
		devices_Unmount ();
		grlib_Exit ();
		BootExecFile ();
		}
		
	if (pln.bootMode == PLN_BOOT_SM)
		{
		grlib_Exit ();
		devices_Unmount ();
		BootToMenu ();
		}

	if (pln.bootMode == PLN_BOOT_NEEK2O)
		{
		Neek2oLoadKernel ();
		grlib_Exit ();
		devices_Unmount ();
		Neek2oBoot ();
		}
	}

//=============================================================================================================================

int cb_Mount (void)
	{
	static time_t t = 0;
	
	if (t == 0) 
		t = time(0);
	
	int elapsed = time(0) - t;
	int remaining = USBTOUT - elapsed;
	
	if (elapsed > 5 && remaining > 0)
		sprintf (mex2,"Mounting USB devices: %d seconds remaining...", remaining);

	Redraw ();
	return 1;
	}

int main(int argc, char **argv) 
	{
	char path[128];
	
	ios_ReloadIOS (58, NULL);
	
	DebugStart (true, NULL);

	Debug ("\nPRIIBOOTERGUI"VER"\n");

	Initialize ();
	
	strcpy (mex1,"postLoader (priibooterGUI "VER")");
	strcpy (mex2,"by stfour");
	
	__exception_setreload(3);
	
	grlibSettings.autoCloseMenu = 20;
	grlib_SetRedrawCallback (Redraw, NULL);
	
	fadeIn ();

	// Let's mount devices
	showHddIcon = 1;
	devices_Mount (DEVMODE_IOS, USBTOUT, cb_Mount);
	showHddIcon = 0;

	*dev = '\0';
	*cfg = '\0';
	
	if (devices_Get (DEV_SD))
		{
		ScanForTheme (DEV_SD);

		strcpy (dev, devices_Get (DEV_SD)); // assign default device
		
		sprintf (path, "%s://ploader", dev);
		if (!fsop_DirExist (path)) fsop_MakeFolder (path);
			
		sprintf (cfg, "%s://ploader//plneek.dir", dev);
		LoadPLN (cfg);
		
		if (fsop_FileExist (POSTLOADER_SDAPP)) pl_sd = 1;
		}
	
	if (devices_Get (DEV_USB))
		{
		if (themeLoaded == 0) ScanForTheme (DEV_USB);
		
		if (*dev == '\0') 
			{
			strcpy (dev, devices_Get (DEV_USB)); // assign default device

			sprintf (path, "%s://ploader", dev);
			if (!fsop_DirExist (path)) fsop_MakeFolder (path);

			sprintf (cfg, "%s://ploader//plneek.dir", dev);
			LoadPLN (cfg);
			}

		if (fsop_FileExist (POSTLOADER_USBAPP))	pl_usb = 1;
		}
	
	if (pl_sd == 0 && pl_usb == 0)
		{
		grlib_menu ("Warning: postLoader.dol not found on sd/usb", " Ok ");
		
		if (pln.bootMode != PLN_BOOT_NEEK)
			keypressed = 1;
		}

	// On the sd we should have plneek.dat... it will instruct us what to do
	nandSource = NULL;

	if (devices_Get (DEV_SD))
		{
		//printd ("Checking "PLNEEK_SDDAT"");
		nandSource = (char*)fsop_ReadFile (PLNEEK_SDDAT, 0, NULL);
		}
	if (!nandSource && devices_Get (DEV_USB))
		{
		//printd ("Checking "PLNEEK_SDDAT"");
		nandSource = (char*)fsop_ReadFile (PLNEEK_USBDAT, 0, NULL);
		}
		
	if (nandSource)
		{
		GetNandsFolders ();

		char *p;
		
		p = nandSource+strlen(nandSource)-1;
		while (*p != '/' && p > nandSource) p--;
		if (*p == '/') p++;
		
		strcpy (pln.name, p);
		//printd (PLNEEK_SDDAT" found -> '%s'", nandSource);

		// If nandSource isn't null, we have to copy selected nand. But before check if we have a nand on the root... if exist it must be backed up
		StoreCurrentNand ();
			
		// Now we can move back the selected nand
		MoveNewNand ();
		}
	
	// After read remove the configuration file. If something fail, doesn't do it again
	remove (PLNEEK_SDDAT);

	GetNandsFolders ();

	Boot ();	

	WII_Initialize();
	WII_LaunchTitle(TITLE_ID(0x00010001,0xAF1BF516)); // HBC v1.0.7+
	WII_LaunchTitle(TITLE_ID(0x00010001,0x4a4f4449)); // HBC JODI
	WII_LaunchTitle(TITLE_ID(0x00010001,0x48415858)); // HBC HAXX 
	
    exit(0);  // Use exit() to exit a program, do not use 'return' from main()
	}

