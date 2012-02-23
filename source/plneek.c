#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>

#include "globals.h"

s_plneek *pln;

bool plneek_GetNandName (void)
	{
	FILE *f;

	*vars.neekName = '\0';
	
	f = fopen (PLNEEK_SDDIR, "rb");
	if (!f) 
		return FALSE;
	
	fread (vars.neekName, PLN_NAME_LEN, 1, f);
	fclose (f);
	
	return TRUE;
	}

static bool GetNandDir (void)
	{
	FILE *f;
	
	f = fopen (PLNEEK_SDDIR, "rb");
	if (!f) f = fopen (PLNEEK_USBDIR, "rb");

	if (!f) return FALSE;
	
	fread (pln, sizeof(s_plneek), 1, f);
	fclose (f);
	
	return TRUE;
	}

static bool WriteDatFile (char *buff)
	{
	FILE *f;
	
	f = fopen (PLNEEK_SDDAT, "wb");
	if (!f) f = fopen (PLNEEK_USBDAT, "wb");
	
	if (!f) return FALSE;

	fwrite (buff, strlen(buff)+1, 1, f); // Add 0 term
	fclose (f);
	
	return TRUE;
	}

bool plneek_ShowMenu (void)
	{
	char *menu;
	char path[64];
	
	pln = calloc (sizeof(s_plneek), 1);
	
	menu = calloc (2048, 1);
	
	int gnd = GetNandDir ();
	if (!gnd || pln->nandsCnt == 0)
		{
		*menu = '\0';
		grlib_menuAddItem (menu, 0, "Continue");
		
		char title[300];
		sprintf (title, 
			"Cannot open "PLNEEK_SDDIR"\n\nUNEEK nands folder must be\nin usb://nands\n\nOr plneek.dol isn't in sd root\nor priibooter.dol isn't updated (%d,%d)",
			gnd, pln->nandsCnt);
			
		grlib_menu (title, menu);
		
		free (menu);
		free (pln);
		return FALSE;
		}
	
	// Ok... we can compose the menu' with nand voices
	int i, id, ret;
	
	id = 100;
	*menu = '\0';
	
	for (i = 0; i < pln->nandsCnt; i++)
		{
		if (pln->nands[i].count)
			{
			grlib_menuAddItem (menu, id, pln->nands[i].path);
			}
		id++;
		}
	
	grlib_menuAddSeparator (menu);

	grlib_menuAddItem (menu, 0, "Cancel");
	
	ret = grlib_menu ("Please select your folder", menu);
	
	*path = '\0';
	if (ret >= 100)
		{
		sprintf (path, pln->nands[ret-100].path);
		if (WriteDatFile (path))
			{
			char buff[300];
			
			sprintf (buff, "Your new NAND is selected\n\nWII Will now reboot\nNAND: %s", path);
			grlib_menu (buff, "OK");

			Shutdown (0);
			SYS_ResetSystem(SYS_RESTART,0,0);
			}
		else
			{
			grlib_menu ("There was an error updating "PLNEEK_SDDAT, "OK");
			}
		}
	
	free (menu);
	free (pln);
	
	// Let's save back
		
	
	return TRUE;
	}