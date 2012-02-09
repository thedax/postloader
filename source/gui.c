/*
GUI

Inside all function to configure  aspect of pl
*/

#include "globals.h"
#include "grlib/grlib.h"
#include "gui.h"

s_gui gui;

//void gui_TopLine

void gui_Init (void)
	{
	memset (&gui, 0, sizeof (s_gui));
	}
	
void gui_Clean (void)
	{
	int i;
	for (i = 0; i < SPOTSMAX; i++)
		{
		//if (gui.spots[i].ico.icon) GRRLIB_FreeTexture (gui.spots[i].ico.icon);
		gui.spots[i].ico.icon = NULL;
		gui.spots[i].id = -1;
		}
	}
	
/////////////////////////////////////////////////////////////////////////

int ChooseDPadReturnMode (u32 btn)
	{
	if (btn & WPAD_BUTTON_UP) // WII Games
		{
		config.gameMode = 0;
		return INTERACTIVE_RET_TOGAMES;
		}
	if (btn & WPAD_BUTTON_DOWN) // GC Games
		{
		config.gameMode = 1;
		return INTERACTIVE_RET_TOGAMES;
		}
	if (btn & WPAD_BUTTON_LEFT) // Homebrew
		{
		return INTERACTIVE_RET_TOHOMEBREW;
		}
	if (btn & WPAD_BUTTON_RIGHT) // Channels
		{
		return INTERACTIVE_RET_TOCHANNELS;
		}
	
	return -1;
	}
	
int Menu_SelectBrowsingMode (void)
	{
	char buff[512];
	buff[0] = '\0';
	
	strcat (buff, "Homebrew mode##100|");
	strcat (buff, "Channel mode##101|");
	strcat (buff, "WII Game mode##102|");
	strcat (buff, "GC Game mode##103|");
	
	strcat (buff, "|");
	strcat (buff, "Close##-1");
	
	grlibSettings.fontNormBMF = fonts[FNTBIG];
	int ret = grlib_menu ("Select browsing mode...", buff);
	grlibSettings.fontNormBMF = fonts[FNTNORM];
	
	if (ret == 100) return INTERACTIVE_RET_TOHOMEBREW;
	if (ret == 101) return INTERACTIVE_RET_TOCHANNELS;
	if (ret == 102) {config.gameMode = GM_WII; return INTERACTIVE_RET_TOGAMES;}
	if (ret == 103) {config.gameMode = GM_DML; return INTERACTIVE_RET_TOGAMES;}
	
	return -1;
	}