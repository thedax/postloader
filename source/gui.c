/*
GUI

Inside all function to configure  aspect of pl
*/
#include "grlib/grlib.h"
#include "gui.h"

s_gui gui;

void gui_Init (void)
	{
	memset (&gui, 0, sizeof (s_gui));
	}
	
void gui_Clean (void)
	{
	int i;
	for (i = 0; i < SPOTSMAX; i++)
		{
		if (gui.spots[i].ico.icon) GRRLIB_FreeTexture (gui.spots[i].ico.icon);
		gui.spots[i].ico.icon = NULL;
		gui.spots[i].id = -1;
		}
	}