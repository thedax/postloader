#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>

#include "wiiload/wiiload.h"
#include "globals.h"
#include "ios.h"
#include "hbcstub.h"
#include "identify.h"
#include "neek.h"

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