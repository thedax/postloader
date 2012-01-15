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
#include "fsop.h"
#include "cfg.h"

#define FNTNORM 0
#define FNTSMALL 1

#define VER "1.0"

#define BASEPATH "usb://nands"
#define PRII_WII_MENU 0x50756E65

#define POSTLOADER_SD "sd://postloader.dol"
#define POSTLOADER_USB "usb://postloader.dol"

#define POSTLOADER_SDAPP "sd://apps/postloader/boot.dol"
#define POSTLOADER_USBAPP "usb://apps/postloader/boot.dol"

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
static int fadeInMsec = 0;

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

static void Redraw (void) // Refresh screen
	{
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
			
		strcat (buff, ": Press any key to change");
			
		grlib_printf (320, 440, GRLIB_ALIGNCENTER, 0, buff);
		}
	else
		grlib_printf (320, 440, GRLIB_ALIGNCENTER, 0, "(Key pressed: Boot menu will be displayed)");
		
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
	
int fadeIn (int init) 
	{
	if (init) alphaback = 0;
	if (alphaback > 255) 
		{
		alphaback = 255;
		return 1; // done
		}

	Redraw ();
	
	usleep (fadeInMsec * 1000);
	
	alphaback += 1;
	
	return 0;
	}

void fadeOut(void) 
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
	cfg = (char*)fsop_GetBuffer (path, NULL, NULL);

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
bool SavePLN (void)
	{
	FILE *f;

	//printd ("Storing nand folder informations");

	f = fopen (PLNEEK_DIR, "wb");
	if (!f) return FALSE;
	
	fwrite (&pln, sizeof(s_plneek), 1, f);
	fclose (f);
	
	return TRUE;
	}

bool LoadPLN (void)
	{
	FILE *f;

	f = fopen (PLNEEK_DIR, "rb");
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
		grlib_menuAddCheckItem (menu, 101, (pln.bootMode == PLN_BOOT_NEEK), "NEEK");
		grlib_menuAddCheckItem (menu, 102, (pln.bootMode == PLN_BOOT_SM), "WII System menu");
		grlib_menuAddCheckItem (menu, 103, (pln.bootMode == PLN_BOOT_HBC), "Homebrew Channel");
		grlib_menuAddSeparator (menu);
		grlib_menuAddItem (menu, 1, "Start selected item");
		grlib_menuAddItem (menu, 2, "Save and start selected item");
		
		ret = grlib_menu ("Please select your option", menu);
		
		if (ret == 100) pln.bootMode = PLN_BOOT_REAL;
		if (ret == 101) pln.bootMode = PLN_BOOT_NEEK;
		if (ret == 102) pln.bootMode = PLN_BOOT_SM;
		if (ret == 103) pln.bootMode = PLN_BOOT_HBC;
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
		if (ChooseNewMode () == 2) SavePLN ();
		}
		
	if (grlibSettings.wiiswitch_reset)
		pln.bootMode = PLN_BOOT_SM;
	
	fadeOut(); // Fade out in an orgy of awesomeness

	WPAD_Shutdown();
	PAD_Reset(0xf0000000);

	//grlib_dosm ("pln.bootMode = %d", pln.bootMode);

	if (pln.bootMode == PLN_BOOT_NEEK)
		{
		Fat_Unmount ();
		grlib_Exit ();
		IOS_ReloadIOS (254); 
		}

	if (pln.bootMode == PLN_BOOT_REAL)
		{
		bool found = FALSE;
		
		if (pl_sd)
			{
			if (!found) found = LoadExecFile (POSTLOADER_SD, "priibooter");
			if (!found) found = LoadExecFile (POSTLOADER_SDAPP, "priibooter");
			}
		if (pl_usb)
			{
			if (!found) found = LoadExecFile (POSTLOADER_USB, "priibooter");
			if (!found) found = LoadExecFile (POSTLOADER_USBAPP, "priibooter");
			}

		if (!found) BootToMenu ();
				
		//grlib_dosm ("found = %d", found);

		Fat_Unmount ();
		grlib_Exit ();
		BootExecFile ();
		
		//grlib_dosm ("failed = %d", found);
		}
		
	if (pln.bootMode == PLN_BOOT_SM)
		{
		grlib_Exit ();
		BootToMenu ();
		}
		
	exit (0); // This isn't really needed...
	}

//=============================================================================================================================

int main(int argc, char **argv) 
	{
	Initialize ();
	
	fadeInMsec = 15;

	strcpy (mex1,"postLoader (priibooterGUI "VER")");
	strcpy (mex2,"by stfour");
	
	__exception_setreload(3);
	
	grlibSettings.autoCloseMenu = 20;
	grlib_SetRedrawCallback (Redraw, NULL);
	Redraw ();
	
	// Let's mount devices
	int sd,usb;
	
	sd = Fat_Mount (DEV_SD, NULL);
	if (sd)
		{
		ScanForTheme (DEV_SD);

		if (sd && !fsop_DirExist ("sd://ploader"))
			mkdir ("sd://ploader", S_IREAD | S_IWRITE);
			
		LoadPLN ();
		}
	
	usb = Fat_Mount (DEV_USB, &keypressed);
	if (usb && themeLoaded == 0)
		{
		ScanForTheme (DEV_USB);
		}
	
	fadeInMsec = 5;
	while (!fadeIn (0));
	
	if (sd && fsop_FileExist (POSTLOADER_SD))
		pl_sd = 1;

	if (sd && fsop_FileExist (POSTLOADER_SDAPP))
		pl_sd = 1;

	if (usb && fsop_FileExist (POSTLOADER_USB))
		pl_usb = 1;

	if (usb && fsop_FileExist (POSTLOADER_USBAPP))
		pl_usb = 1;

	if (pl_sd == 0 && pl_usb == 0)
		{
		printd ("Warning: postLoader.dol not found on sd/usb");
		
		if (pln.bootMode != PLN_BOOT_NEEK)
			keypressed = 1;
		}

	// On the sd we should have plneek.dat... it will instruct us what to do
	nandSource = NULL;

	if (sd)
		{
		//printd ("Checking "PLNEEK_DAT"");
		nandSource = (char*)fsop_GetBuffer (PLNEEK_DAT, NULL, NULL);
		}
		
	if (nandSource)
		{
		GetNandsFolders ();

		char *p;
		
		p = nandSource+strlen(nandSource)-1;
		while (*p != '/' && p > nandSource) p--;
		if (*p == '/') p++;
		
		strcpy (pln.name, p);
		//printd (PLNEEK_DAT" found -> '%s'", nandSource);

		// If nandSource isn't null, we have to copy selected nand. But before check if we have a nand on the root... if exist it must be backed up
		StoreCurrentNand ();
			
		// Now we can move back the selected nand
		MoveNewNand ();
		}
	
	// After read remove the configuration file. If something fail, doesn't do it again
	remove (PLNEEK_DAT);

	GetNandsFolders ();

	Boot ();	
	
    exit(0);  // Use exit() to exit a program, do not use 'return' from main()
	}