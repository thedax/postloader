#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <dirent.h>
#include <wiiuse/wpad.h>
#include <sys/unistd.h>
#include "common.h"

#define VER "v1.10"
#define BASEPATH "usb://nands"
#define PRII_WII_MENU 0x50756E65

#define POSTLOADER_SD "sd://postloader.dol"
#define POSTLOADER_USB "usb://postloader.dol"

#define POSTLOADER_SDAPP "sd://apps/postloader/boot.dol"
#define POSTLOADER_USBAPP "usb://apps/postloader/boot.dol"

bool LoadExecFile (char *path, char *args);
bool BootExecFile (void);

int ios_ReloadIOS(int ios, int *ahbprot);
bool IsNandFolder (char *path);
void green_fix(void); //GREENSCREEN FIX

static char *nandSource;
int errors = 0;
s_plneek pln; //pln=plneek

static int pl_sd = 0;
static int pl_usb = 0;
static int keypressed = 0;

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

/*
this function will retrive all available nand foder in usb://nands
*/	
void GetNandsFolders (void)
	{
	DIR *pdir;
	struct dirent *pent;
	
	char path[256];

	// As there is a nand, we must found the empty folder in usb://nands that was containing it...
	printd ("Scanning for nand folders...");
	
	pln.nandsCnt = 0;

	strcpy (path, BASEPATH);
	
	pdir=opendir(path);
	while ((pent=readdir(pdir)) != NULL) 
		{
		//printd ("    '%s'\n", pent->d_name);
		sprintf (path, BASEPATH"/%s", pent->d_name);
		
		// Let's check if we have really a folder
		if (strcmp (pent->d_name, ".") != 0 &&
			strcmp (pent->d_name, "..") != 0 &&
			DirExist (path))
			{
			strcpy (pln.nands[pln.nandsCnt].path, path);
			pln.nands[pln.nandsCnt].count = CountObjectsInFolder (pln.nands[pln.nandsCnt].path);
			
			if (pln.nands[pln.nandsCnt].count == 0 || IsNandFolder (pln.nands[pln.nandsCnt].path))
				{
				//printd ("    %s: %d\n", pln.nands[pln.nandsCnt].path, pln.nands[pln.nandsCnt].count);
				pln.nandsCnt++; 	// Added
				}
			}
		}
	closedir(pdir);
	
	printd ("%d found\n", pln.nandsCnt);
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
		
		if (DirExist (source))
			{
			if (rename (source, path) == 0)
				printd (".");
			else
				{
				printd ("\n   Error on mooving %s to %s\n", nandsubs[i], path);
				sleep (5);
				errors++;
				}
			}
		}
	
	printd ("\n");
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
		
		if (DirExist (source))
			{
			if (rename (source, path) == 0)
				printd (".");
			else
				{
				printd ("\n   Error on mooving %s to %s\n", nandsubs[i], path);
				errors++;
				}
			}
		}
	
	printd ("\n");
	}
	
/*
As pl have no accesso to usb, we need to store the available nands to sd
*/
bool SavePLN (void)
	{
	FILE *f;

	printd ("Storing nand folder informations\n");

	f = fopen (PLNEEK_DIR, "wb");
	if (!f) return FALSE;
	
	fwrite (&pln, sizeof(s_plneek), 1, f);
	fclose (f);
	
	return TRUE;
	}

bool LoadPLN (void)
	{
	FILE *f;

	printd ("Retriving current nand folder informations\n");

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
	
bool WaitForAnyKey (void)
	{
	u32  wbtn, gcbtn;
	int i;
	bool pressed = false;
	char mask[80];
	char buff[80];
	
	strcpy (mask, "---------------------------------------------------------------------------");
	for (i = 0; i <strlen (mask); i++)
		{
		strcpy (buff, mask);
		buff[i] = '*';
		printf ("%s\r", buff);
		VIDEO_WaitVSync();

		WPAD_ScanPads();  // Scan the Wiimotes
		wbtn = WPAD_ButtonsDown(0);

		PAD_ScanPads();
		gcbtn = PAD_ButtonsDown(0);
		
		if (wbtn || gcbtn)
			{
			pressed = true;
			break;
			}
		usleep (20 * 1000);
		}
	printd ("---------------------------------------------------------------------------\n");
	
	return pressed;
	}
	
void ChooseNewMode (void)
	{
	bool pressed = false;
	u32  wbtn, gcbtn;
	time_t tout;

	printd ("       Make your boot choice (press DPAD on wiimote or gc controller)      \n");
	printf ("                                                                           \n");
	printf ("                              postLoader [UP]                              \n");
	printf ("                                    |                                      \n");
	printf ("               System Menu [LEFT] --+-- Homebrew Channel [RIGHT]           \n");
	printf ("                                    |                                      \n");
	printf ("                                UNEEK [DOWN]                               \n");
	printf ("                                                                           \n");

	tout = time(NULL) + 10;
	while (!pressed && time(NULL)<tout)
		{
		printd ("                      (%d second to make a selection...)                   \r", tout - time(NULL));

		WPAD_ScanPads();  // Scan the Wiimotes
		wbtn = WPAD_ButtonsDown(0);

		PAD_ScanPads();
		gcbtn = PAD_ButtonsDown(0);

		if (wbtn)
			{
			// Wait until button is released
			while (WPAD_ButtonsDown(0)) WPAD_ScanPads();
			}

		// Then gc
		if (gcbtn)
			{
			// Wait until button is released
			while (PAD_ButtonsDown(0)) PAD_ScanPads();
			}
			
		if (wbtn & WPAD_BUTTON_UP || gcbtn & PAD_BUTTON_UP)
			{
			pln.bootMode = PLN_BOOT_REAL;
			pressed = TRUE;
			}

		if (wbtn & WPAD_BUTTON_DOWN || gcbtn & PAD_BUTTON_DOWN)
			{
			pln.bootMode = PLN_BOOT_NEEK;
			pressed = TRUE;
			}

		if (wbtn & WPAD_BUTTON_LEFT || gcbtn & PAD_BUTTON_LEFT)
			{
			pln.bootMode = PLN_BOOT_SM;
			pressed = TRUE;
			}

		if (wbtn & WPAD_BUTTON_RIGHT || gcbtn & PAD_BUTTON_RIGHT)
			{
			pln.bootMode = PLN_BOOT_HBC;
			pressed = TRUE;
			}
		}

	printd ("                                                                           \n");
	}

void Boot (void)
	{
	printd ("---------------------------------------------------------------------------\n");
	
	if (pln.bootMode == PLN_BOOT_REAL)
		printd ("       You are in REAL NAND mode: Press any key NOW for boot options\n");
	if (pln.bootMode == PLN_BOOT_NEEK)
		printd ("         You are in UNEEK mode: Press any key NOW for boot options\n");
	if (pln.bootMode == PLN_BOOT_SM)
		printd ("      You are in System Menu' mode: Press any key NOW for boot options\n");
	if (pln.bootMode == PLN_BOOT_HBC)
		printd ("     You are in HomeBrewChannel mode: Press any key NOW for boot options\n");

	if (keypressed || WaitForAnyKey ())
		{
		ChooseNewMode ();
		}
		
	printd ("\n");

	SavePLN ();
	
	WPAD_Shutdown();
	PAD_Reset(0xf0000000);

	//if (IsNandFolder ("usb:/") && pln.bootMode == PLN_BOOT_NEEK)
	if (pln.bootMode == PLN_BOOT_NEEK)
		{
		printd ("Booting neek...");
		Fat_Unmount ();
		green_fix ();
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
				
		Fat_Unmount ();
		green_fix ();
		BootExecFile ();
		}
		
	if (pln.bootMode == PLN_BOOT_SM)
		{
		green_fix ();
		BootToMenu ();
		}
		
	exit (0); // This isn't really needed...
	}
	
void LoadIOS58 (void)
	{
	printd ("Loading IOS58...");
	if (ios_ReloadIOS (58, NULL) == 58)
		printd ("Ok\n");
	else
		{
		printd ("Fail\n");
		}
	}
	
void HBC (void)
	{
	exit(0);
	}
 
//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
	{
	InitVideo ();

	printf ("\n");
	printd ("---------------------------------------------------------------------------\n");
	printd ("                        priibooter "VER" by stfour\n");
	printd ("                       (part of postLoader project)\n");
	printd ("---------------------------------------------------------------------------\n");
	
	PAD_Init(); 
	WPAD_Init();

	// Let's mount devices
	int sd,usb;
	
	sd = Fat_Mount (DEV_SD, &keypressed);
	usb = Fat_Mount (DEV_USB, &keypressed);
	
	if (sd == 0 && usb == 0)
		{
		printd ("SD & USB not available, booting to System Menu\n");
		sleep(3);
		BootToMenu ();
		}

	if (sd && FileExist (POSTLOADER_SD))
		pl_sd = 1;

	if (sd && FileExist (POSTLOADER_SDAPP))
		pl_sd = 1;

	if (usb && FileExist (POSTLOADER_USB))
		pl_usb = 1;

	if (usb && FileExist (POSTLOADER_USBAPP))
		pl_usb = 1;

	
	printf ("\n");
	
	if (sd && !DirExist ("sd://ploader"))
		mkdir ("sd://ploader", S_IREAD | S_IWRITE);
		
	LoadPLN ();

	if (pl_sd == 0 && pl_usb == 0)
		{
		printd ("postLoader.dol not found on sd/usb");
		
		if (pln.bootMode != PLN_BOOT_NEEK)
			keypressed = 1;
		}

	// On the sd we should have plneek.dat... it will instruct us what to do
	nandSource = NULL;

	if (sd)
		{
		printd ("Checking "PLNEEK_DAT"\n");
		nandSource = (char*)ReadFile2Buffer (PLNEEK_DAT, NULL, NULL);
		}
		
	if (nandSource)
		{
		GetNandsFolders ();

		char *p;
		
		p = nandSource+strlen(nandSource)-1;
		while (*p != '/' && p > nandSource) p--;
		if (*p == '/') p++;
		
		strcpy (pln.name, p);
		printd (PLNEEK_DAT" found -> '%s'\n", nandSource);

		// If nandSource isn't null, we have to copy selected nand. But before check if we have a nand on the root... if exist it must be backed up
		StoreCurrentNand ();
			
		// Now we can move back the selected nand
		MoveNewNand ();
		}
	
	// After read remove the configuration file. If something fail, doesn't do it again
	remove (PLNEEK_DAT);

	GetNandsFolders ();
	Boot ();
		
	exit (0);
	}
