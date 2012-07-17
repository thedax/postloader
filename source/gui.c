/*
GUI

Inside all function to configure  aspect of pl
*/

#include "globals.h"
#include "grlib/grlib.h"
#include "gui.h"
#include "devices.h"

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
		if (config.browseMode == BROWSE_GM)
			{
			if (config.gameMode == 0)
				config.gameMode = 1;
			else
				config.gameMode = 0;
			}
		return INTERACTIVE_RET_TOGAMES;
		}
	if (btn & WPAD_BUTTON_DOWN) // GC Games
		{
		return INTERACTIVE_RET_TOEMU;
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
/*
int ChooseDPadReturnMode (u32 btn)
	{
	if (btn & WPAD_BUTTON_DOWN) // Homebrew
		{
		if (config.browseMode == BROWSE_HB)
			{
			return INTERACTIVE_RET_TOEMU;
			}
		if (config.browseMode == BROWSE_EM)
			{
			config.gameMode = 1;
			return INTERACTIVE_RET_TOGAMES;
			}
		if (config.browseMode == BROWSE_GM && config.gameMode == 1)
			{
			config.gameMode = 0;
			return INTERACTIVE_RET_TOGAMES;
			}
		if (config.browseMode == BROWSE_GM && config.gameMode == 0)
			{
			return INTERACTIVE_RET_TOCHANNELS;
			}
		if (config.browseMode == BROWSE_CH)
			{
			return INTERACTIVE_RET_TOHOMEBREW;
			}
		}
	if (btn & WPAD_BUTTON_UP) // Channels
		{
		if (config.browseMode == BROWSE_HB)
			{
			return INTERACTIVE_RET_TOCHANNELS;
			}
		if (config.browseMode == BROWSE_CH)
			{
			config.gameMode = 0;
			return INTERACTIVE_RET_TOGAMES;
			}
		if (config.browseMode == BROWSE_GM && config.gameMode == 0)
			{
			config.gameMode = 1;
			return INTERACTIVE_RET_TOGAMES;
			}
		if (config.browseMode == BROWSE_GM && config.gameMode == 1)
			{
			return INTERACTIVE_RET_TOEMU;
			}
		if (config.browseMode == BROWSE_EM)
			{
			return INTERACTIVE_RET_TOHOMEBREW;
			}
		}
	
	return -1;
	}
*/	
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
	
	int ret = grlib_menu ("Select browsing mode...", buff);
	
	if (ret == 100) return INTERACTIVE_RET_TOHOMEBREW;
	if (ret == 101) return INTERACTIVE_RET_TOCHANNELS;
	if (ret == 102) {config.gameMode = GM_WII; return INTERACTIVE_RET_TOGAMES;}
	if (ret == 103) {config.gameMode = GM_DML; return INTERACTIVE_RET_TOGAMES;}
	
	return -1;
	}
	
int GoToPage (int page, int pageMax)
	{
	int cols;
	int col, i, newpage, inc;
	char buff[4096];
	
	*buff = '\0';
	
	if (pageMax < 100)
		{
		cols = 10;
		inc = 1;
		}
	else if (pageMax < 500)
		{
		cols = 10;
		inc = 5;
		}
	else if (pageMax < 2500)
		{
		cols = 10;
		inc = 25;
		}
	else
		{
		cols = 10;
		inc = 100;
		}
	
	for (col = 0; col < cols; col++)
		{
		for (i = 0; i < pageMax / inc; i ++)
			{
			newpage = (col + (i * cols)) * inc;
			if (inc > 1 && newpage > 1) newpage --;
			if (newpage <= pageMax)
				{
				grlib_menuAddItem (buff, newpage, "%d", newpage+1);
				}
			}
		
		if (col < cols -1) grlib_menuAddColumn (buff);
		}
	int item = grlib_menu ("Go to page", buff);
	if (item >= 0) page = item;
	
	return page;
	}

#define TOPBARITEMS 5	
#define BTNOFFSET 15
#define TOPBARH 160

int DrawTopBar (int *visibleflag, int *browserRet, u32 *btn, int *closed)
	{
	s_grlibobj go;
	static int visible = 0;
	static int y = -TOPBARH;
	int ret = -1;
	int br = -1;
	
	if (browserRet && *browserRet > 0) return 0;
	
	if (visible == 0 && grlib_irPos.y < 40 && grlib_irPos.y > 0) visible = 1;
	if (visible == 2 && grlib_irPos.y > (TOPBARH - 20))	visible = 3;
	if (visible == 1 && y < -30) y+=10;
	if (visible == 1 && y >= -30) visible = 2;
	if (visible == 3) y-=5;

	if (y <= -TOPBARH)
		{
		visible = 0;
		if (closed) *closed = 1;
		y = -TOPBARH;
		}
	else
		{
		if (closed) *closed = 0;
		}
	
	go.y1 = y;
	go.y2 = y + TOPBARH;
	
	go.bcolor = RGBA (32, 32, 32, 192);
	go.color = RGBA (192, 192, 192, 255);
	
	if (visible)
		{
		Video_SetFont (TTFSMALL);
		
		static int init = 0;
		static s_grlibobj goItems[TOPBARITEMS];
		
		if (!init)
			{
			strcpy (goItems[0].text, "Homebrews");
			strcpy (goItems[1].text, "Channels");
			strcpy (goItems[2].text, "WII Games");
			strcpy (goItems[3].text, "GC Games");
			strcpy (goItems[4].text, "Emulators");
			init = 1;
			}
		
		int i;
		int x;
		int barWidth;
		int btnHeight = 90;
		int btnWidth = 0;
		
		// Let's measure max button width
		for (i = 0; i < TOPBARITEMS; i++)
			{
			int w;
			
			w = grlib_GetFontMetrics (goItems[i].text, NULL, NULL);
			if (w > btnWidth) btnWidth = w;
			}
		
		//btnWidth = 60;
		
		go.x1 = 320 - (((btnWidth + BTNOFFSET) * TOPBARITEMS) / 2) - 10;
		go.x2 = 320 + (((btnWidth + BTNOFFSET) * TOPBARITEMS) / 2) + 10;
		barWidth = go.x2 - go.x1;
		
		grlib_DrawWindow (go);
		
		x = go.x1 + (barWidth / 2) - ((((btnWidth + BTNOFFSET) * TOPBARITEMS) / 2) - (BTNOFFSET / 2));
		for (i = 0; i < TOPBARITEMS; i++)
			{
			goItems[i].vAlign = GRLIB_ALIGNBOTTOM;
			goItems[i].x1 = x;
			goItems[i].x2 = x+btnWidth;
			goItems[i].y1 = go.y2 - 10 - btnHeight;
			goItems[i].y2 = go.y2 - 10;
			goItems[i].bcolor = RGBA (64, 64, 64, 128);
			goItems[i].color = RGBA (192, 192, 192, 255);
			
			if (grlib_irPos.x > goItems[i].x1 && grlib_irPos.x < goItems[i].x2 && grlib_irPos.y > goItems[i].y1 && grlib_irPos.y < goItems[i].y2)
				{
				s_grlibobj btnsel;
				grlib_MagObject (&btnsel, &goItems[i], 3, 3);
				grlib_DrawButton (&btnsel, BTNSTATE_SEL);
				
				if (btn && *btn == WPAD_BUTTON_A) ret = i;
				}
			else
				grlib_DrawButton (&goItems[i], BTNSTATE_NORM);
				
			Video_DrawIconZ (TEX_CAT_HB + i, x + btnWidth/2, go.y2 - 65, 1.0, 1.3);
			
			x += (btnWidth + BTNOFFSET);
			}
			
		if (ret == 0) // Homebrew
			{
			br = INTERACTIVE_RET_TOHOMEBREW;
			}
		if (ret == 1) // Channels
			{
			br = INTERACTIVE_RET_TOCHANNELS;
			}
		if (ret == 2) // WII Games
			{
			br = INTERACTIVE_RET_TOGAMES;
			config.gameMode = 0;
			}
		if (ret == 3) // GC Games
			{
			br = INTERACTIVE_RET_TOGAMES;
			config.gameMode = 1;
			}
		if (ret == 4) // GC Games
			{
			br = INTERACTIVE_RET_TOEMU;
			config.gameMode = 1;
			}
		}

	if (br != -1) // something selected
		visible = 3; // Start close

	if (browserRet) *browserRet = br;
	if (visibleflag) *visibleflag = (visible == 2 ? 1 : 0);
	
	return ret;
	}
	
static int BOTBARITEMS = 5;
int DrawBottomBar (int *visibleflag, u32 *btn, int *closed)
	{
	s_grlibobj go;
	static int visible = 0;
	static int y = 480;
	int ret = -1;
	int i;

	static int itemSE = -1, itemWM = -1, itemAbout = -1, itemConfig = -1, itemDisc = -1, itemExit = -1, itemNeek = -1;
	
	if (visible == 0 && grlib_irPos.y > 420) visible = 1;
	if (visible == 1 && y > 355) y-=10;
	if (visible == 1 && y <= 355) visible = 2;
	if (visible == 2 && grlib_irPos.y < 350) visible = 3;
	if (visible == 3) y+=5;

	if (y >= 480)
		{
		visible = 0;
		if (closed) *closed = 1;
		y = 480;
		}
	else
		{
		if (closed) *closed = 0;
		}
		
	go.y1 = y;
	go.y2 = y + TOPBARH;
	
	go.bcolor = RGBA (32, 32, 32, 192);
	go.color = RGBA (192, 192, 192, 255);
	
	if (visible)
		{
		Video_SetFont (TTFSMALL);

		static int init = 0;
		static s_grlibobj goItems[7];
		
		if (!init)
			{
			int dev;
			char path[256];
			
			for (dev = 0; dev < DEV_MAX; dev++)
				{
				if (devices_Get (dev))
					{
					sprintf (path, "%s://apps/SettingsEditorGUI/boot.dol", devices_Get (dev));
					Debug ("DrawBottomBar: searching %s", path);
					if (fsop_FileExist (path))
						{
						strcpy (vars.sePath, path);
						itemSE = 1;
						Debug ("DrawBottomBar: found!");
						BOTBARITEMS ++;
						break;
						}
					sprintf (path, "%s://apps/Settings Editor GUI/boot.dol", devices_Get (dev));
					Debug ("DrawBottomBar: searching %s", path);
					if (fsop_FileExist (path))
						{
						strcpy (vars.sePath, path);
						itemSE = 1;
						Debug ("DrawBottomBar: found!");
						BOTBARITEMS ++;
						break;
						}
					}
				}
			
			for (dev = 0; dev < DEV_MAX; dev++)
				{
				if (devices_Get (dev))
					{
					sprintf (path, "%s://apps/wiimod/boot.dol", devices_Get (dev));
					Debug ("DrawBottomBar: searching %s", path);
					if (fsop_FileExist (path))
						{
						strcpy (vars.wmPath, path);
						itemWM = 1;
						Debug ("DrawBottomBar: found!");
						BOTBARITEMS ++;
						break;
						}
					}
				}

			i = 0;
			
			strcpy (goItems[i].text, "About.."); 	itemAbout = i++;
			strcpy (goItems[i].text, "Config");		itemConfig = i++;
			strcpy (goItems[i].text, "Run Disc");	itemDisc = i++;
			strcpy (goItems[i].text, "Neek");		itemNeek = i++;

			if (itemSE > 0) 
				{
				itemSE = i;
				strcpy (goItems[i++].text, "Setting Ed.");
				}

			if (itemWM > 0) 
				{
				itemWM = i;
				strcpy (goItems[i++].text, "WiiMod");
				}

			strcpy (goItems[i].text, "Exit");		itemExit = i++;

			init = 1;
			}
		
		int x;
		int barWidth;
		int btnHeight = 90;
		int btnWidth = 0;
		
		// Let's measure max button width
		for (i = 0; i < BOTBARITEMS; i++)
			{
			int w;
			
			w = grlib_GetFontMetrics (goItems[i].text, NULL, NULL);
			if (w > btnWidth) btnWidth = w;
			}
		
		if (btnWidth < 60)
			btnWidth = 60;
		
		go.x1 = 320 - (((btnWidth + BTNOFFSET) * (BOTBARITEMS - (vars.neek == NEEK_NONE ? 0 : 1))) / 2) - 10;
		go.x2 = 320 + (((btnWidth + BTNOFFSET) * (BOTBARITEMS - (vars.neek == NEEK_NONE ? 0 : 1))) / 2) + 10;
		barWidth = go.x2 - go.x1;
		
		grlib_DrawWindow (go);
		
		x = go.x1 + (barWidth / 2) - ((((btnWidth + BTNOFFSET) * (BOTBARITEMS - (vars.neek == NEEK_NONE ? 0 : 1))) / 2) - (BTNOFFSET / 2));
		for (i = 0; i < BOTBARITEMS; i++)
			{
			if (vars.neek != NEEK_NONE && i == itemNeek) continue;
			
			goItems[i].vAlign = GRLIB_ALIGNBOTTOM;
			goItems[i].x1 = x;
			goItems[i].x2 = x+btnWidth;
			goItems[i].y1 = go.y1 + 10;
			goItems[i].y2 = go.y1 + 10 + btnHeight;
			goItems[i].bcolor = RGBA (64, 64, 64, 128);
			goItems[i].color = RGBA (192, 192, 192, 255);
			
			if (grlib_irPos.x > goItems[i].x1 && grlib_irPos.x < goItems[i].x2 && grlib_irPos.y > goItems[i].y1 && grlib_irPos.y < goItems[i].y2)
				{
				s_grlibobj btnsel;
				grlib_MagObject (&btnsel, &goItems[i], 3, 3);
				grlib_DrawButton (&btnsel, BTNSTATE_SEL);
				
				if (btn && *btn == WPAD_BUTTON_A) 
					{
					if (i == itemAbout)
						ret = 0;
					else if (i == itemConfig)
						ret = 1;
					else if (i == itemDisc)
						ret = 2;
					else if (i == itemExit)
						ret = 3;
					else if (i == itemNeek)
						ret = 4;
					else if (i == itemSE)
						ret = 5;
					else if (i == itemWM)
						ret = 6;
					}
				}
			else
				grlib_DrawButton (&goItems[i], BTNSTATE_NORM);
				
			if (i == itemSE)
				Video_DrawIconZ (TEX_ICO_SE, x + btnWidth/2, go.y1 + 45, 1.0, 1.3);
			else if (i == itemWM)
				Video_DrawIconZ (TEX_ICO_WM, x + btnWidth/2, go.y1 + 45, 1.0, 1.3);
			else if (i == itemAbout)
				Video_DrawIconZ (TEX_ICO_ABOUT, x + btnWidth/2, go.y1 + 45, 1.0, 1.3);
			else if (i == itemConfig)
				Video_DrawIconZ (TEX_ICO_CONFIG, x + btnWidth/2, go.y1 + 45, 1.0, 1.3);
			else if (i == itemDisc)
				Video_DrawIconZ (TEX_ICO_DVD, x + btnWidth/2, go.y1 + 45, 1.0, 1.3);
			else if (i == itemNeek)
				Video_DrawIconZ (TEX_ICO_NEEK, x + btnWidth/2, go.y1 + 45, 1.0, 1.3);
			else if (i == itemExit)
				Video_DrawIconZ (TEX_ICO_EXIT, x + btnWidth/2, go.y1 + 45, 1.0, 1.3);
			
			x += (btnWidth + BTNOFFSET);
			}
		}

	if (ret != -1) // something selected
		{
		visible = 0;
		y = 480;
		}

	if (visibleflag) *visibleflag = (visible == 2 ? 1 : 0);
	
	return ret;
	}
	
