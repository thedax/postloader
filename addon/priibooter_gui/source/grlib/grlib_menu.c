#include <stdlib.h>
#include <stdarg.h>
#include "grlib.h"
#include <wiiuse/wpad.h>

// This contain extension to draw a menu on screen
// (A) select item, (B) cancel

#define MAXITEMS 128
#define MAXCOLS 12
#define YSPACING 25
#define YSPACINGFAKE 10
#define YTITLE 20
#define XCOLSPACING 10

extern s_grlibSettings grlibSettings;

int grlib_menuAddItem (char *menu, int id, const char *itemsstring, ...)
	{
	char buff[256];
	char sid[16];
	
	if (menu == NULL) return 0;

	if (itemsstring != NULL)
		{
		va_list argp;
		va_start(argp, itemsstring);
		vsprintf(buff, itemsstring, argp);
		va_end(argp);
		}
		
	strcat (menu, buff);
	sprintf (sid, "##%d|", id);
	strcat (menu, sid);
	
	return 1;
	}

int grlib_menuAddSeparator (char *menu)
	{
	if (menu == NULL) return 0;

	strcat (menu, "|");
	return 1;
	}

int grlib_menuAddColumn (char *menu)
	{
	if (menu == NULL) return 0;

	strcat (menu, "~");
	return 1;
	}

int grlib_menuAddCheckItem (char *menu, int id, bool check, const char *itemsstring, ...)
	{
	char buff[256];
	char sid[16];
	
	if (menu == NULL) return 0;

	if (itemsstring != NULL)
		{
		va_list argp;
		va_start(argp, itemsstring);
		vsprintf(buff, itemsstring, argp);
		va_end(argp);
		}
		
	if (check)
		strcat (menu, "^+");
	else
		strcat (menu, "^-");
		
	strcat (menu, buff);
	sprintf (sid, "##%d|", id);
	strcat (menu, sid);
	
	return 1;
	}

int grlib_menuAddCustomCheckItem (char *menu, int id, bool check, const char *yesno, const char *itemsstring, ...)
	{
	char buff[256];
	char sid[16];
	char yn[64];
	
	if (menu == NULL) return 0;

	if (itemsstring != NULL)
		{
		va_list argp;
		va_start(argp, itemsstring);
		vsprintf(buff, itemsstring, argp);
		va_end(argp);
		}

	strcpy (yn, yesno);
	char *p = strstr (yn, "|");
	if (!p) return 0;
	*p = '\0';

	if (check)
		strcat (buff, yn);
	else
		strcat (buff, ++p);

	strcat (menu, buff);
	sprintf (sid, "##%d|", id);
	strcat (menu, sid);
	
	return 1;
	}

int grlib_menu (char *title, const char *itemsstring, ...) // item1|item2|item3... etc,
	{
	int i,j;
	int item = 0;
	int itemsCnt, itemsCntReal[MAXCOLS], itemsCntFake[MAXCOLS];
	int titleh;
	int titlew, itemw, linew, lineh;
	int halfx, halfy;
	int winw, winh;
	int x, y, yTop;
	int columns = 0;
	int titleLines;
	char buff[1024];
	char *line;
	u32 btn;
	time_t t;
	
	int cols[MAXCOLS];
	s_grlibobj goWindow, goItems[MAXITEMS];
	int retcodes[MAXITEMS];
	
	memset (cols, 0, sizeof(cols));
	
	for (i = 0; i < MAXITEMS; i++) retcodes[i] = i;
	
	if (itemsstring != NULL)
		{
		va_list argp;
		va_start(argp, itemsstring);
		vsprintf(buff, itemsstring, argp);
		va_end(argp);
		}
	else return 0;
	
	j = strlen(buff);
	if (buff[j-1] == '|' && j > 2) buff[j-1] = '\0';
	
	if (grlibSettings.RedrawCallback != NULL) grlibSettings.RedrawCallback();
	
	// Grayout background
	goWindow.x1 = 0;
	goWindow.y1 = 0;
	goWindow.x2 = grlib_GetScreenW();
	goWindow.y2 = grlib_GetScreenH();
	goWindow.color = RGBA(0,0,0,128);
	goWindow.bcolor = RGBA(0,0,0,128);
	grlib_DrawSquare (&goWindow);
	
	line = calloc (1, strlen (title) + 1);
	// Lets count tile lines
	titleLines = 0;

	titlew = 0;
	linew = 0;
	lineh = 0;
	
	grlibSettings.fontBMF = grlibSettings.fontNormBMF;

	j = 0; // used to count item chars
	for (i = 0; i <= strlen(title); i++)
		{
		if (title[i] == 0 || title[i] == '\n')
			{
			int l,h;
			strncpy (line, &title[j], i - j);
			line[i - j] = 0;
			grlib_GetFontMetrics (line, &l, &h);
			if (h > lineh) lineh = h;
			if (l > linew) linew = l;
			if (l > titlew) titlew = l;
			j = i + 1;
			
			titleLines++;
			
			// Switch to small fonts
			grlibSettings.fontBMF = grlibSettings.fontSmallBMF;
			}
		}
	
	titlew += 5;

	grlibSettings.fontBMF = grlibSettings.fontNormBMF;

	// Separate passed string to items array... also calculate max width and height in pixels for items
	itemsCnt = 0;
	
	itemw = 0;
	j = 0; // used to count item chars
	memset (itemsCntReal, 0, sizeof (itemsCntReal));
	memset (itemsCntFake, 0, sizeof (itemsCntFake));
	for (i = 0; i <= strlen(buff); i++)
		{
		if (buff[i] == 0 || buff[i] == '|' || buff[i] == '~')
			{
			int l,h;
			char *p;
			
			//if (buff[j] == '~') continue;

			strncpy (goItems[itemsCnt].text, &buff[j], i - j);
			goItems[itemsCnt].text[i - j] = 0;
			
			p = strstr (goItems[itemsCnt].text, "##");
			if (p)
				{
				*p = 0;
				p += 2;
				retcodes[itemsCnt] = atoi(p);
				}
			
			if (goItems[itemsCnt].text[0] == '^')
				{
				char buff[256];
				
				sprintf (buff, "[X] %s", &goItems[itemsCnt].text[2]);
				grlib_GetFontMetrics (buff, &l, &h);
				}
			else
				{
				grlib_GetFontMetrics (goItems[itemsCnt].text, &l, &h);
				}
				
			if (h > lineh) lineh = h;
			if (l > linew) linew = l;
			if (l > itemw) itemw = l;
			
			if (strlen (goItems[itemsCnt].text))
				itemsCntReal[columns] ++;
			else
				itemsCntFake[columns] ++;
				
			itemsCnt++;
				
			cols[columns]++;
			j = i + 1;
			}
		if (buff[i] == '~')
			{
			columns++;
			//grlib_dosm ("c = %d", columns);
			j = i + 1;
			
			strcpy (goItems[itemsCnt].text, "~");
			retcodes[itemsCnt] = 0;
			itemsCnt++;
			}
		if (itemsCnt == MAXITEMS) break;
		}
		
	columns++;
	
	//grlib_dosm ("c = %d", columns);
	
	for (i = 0; i < columns; i++)
		itemsCntFake[i]--;
		
	//if (itemsCntFake < 0) itemsCntFake = 0;
	
	titleh = ((titleLines+1) * (lineh + 5));
	
	itemw += 10;
	winw = ((itemw+XCOLSPACING) * (columns)) + 40;
	if (linew > winw)
		winw = linew + 40;
		
	if (winw < titlew)
		winw = titlew;
	
	winh = 0;
	for (i = 0; i < columns; i++)
		{
		j = (itemsCntReal[i] * (lineh + YSPACING)) + (itemsCntFake[i] * YSPACINGFAKE) + titleh + YSPACINGFAKE;
		if (j > winh)
			winh = j;
		}
	
	halfx = rmode->fbWidth / 2;
	halfy = rmode->efbHeight / 2;
	
	goWindow.x1 = halfx - (winw / 2);
	goWindow.x2 = halfx + (winw / 2);
	
	goWindow.y1 = halfy - (winh / 2);
	goWindow.y2 = halfy + (winh / 2);
	
	goWindow.bcolor = RGBA (32, 32, 32, 192);
	goWindow.color = RGBA (192, 192, 192, 255);
	
	if (grlibSettings.theme.enabled)
		{
		grlib_DrawSquareThemed (&goWindow, grlibSettings.theme.texWindow, grlibSettings.theme.texWindowBk, grlibSettings.theme.windowMagX, grlibSettings.theme.windowMagY, DSTF_BKFILLBORDER);
		}
	else
		{
		grlib_DrawSquare (&goWindow);
		grlib_DrawBoldEmptySquare (&goWindow);
		}
	
	// ... so, draw the title...
	y = goWindow.y1 + 5;
	j = 0; // used to count item char
	for (i = 0; i <= strlen(title); i++)
		{
		if (title[i] == 0 || title[i] == '\n')
			{
			int l,h;
			strncpy (line, &title[j], i - j);
			line[i - j] = 0;
			grlib_GetFontMetrics (line, &l, &h);
			if (h > lineh) lineh = h;
			if (l > linew) linew = l;
			j = i + 1;
			if (titleLines > 1)
				grlib_printf ( goWindow.x1 + 5, y, GRLIB_ALIGNLEFT, 0, line);
			else
				grlib_printf ( halfx, y, GRLIB_ALIGNCENTER, 0, line);

			y += (lineh + 5);
			
			grlibSettings.fontBMF = grlibSettings.fontSmallBMF;
			}
		}
	grlibSettings.fontBMF = grlibSettings.fontNormBMF;

	y += YSPACINGFAKE;
	yTop = y;
	x = halfx - ((columns - 1) * ((itemw+10) / 2));
	for (i = 0; i < itemsCnt; i++)
		{
		if (strcmp (goItems[i].text, "~") == 0)
			{
			memset (&goItems[i], 0, sizeof (s_grlibobj));
			x += (itemw + XCOLSPACING);
			y = yTop;
			}
		else if (strlen(goItems[i].text))
			{
			goItems[i].x1 = x - (itemw / 2);
			goItems[i].x2 = x + (itemw / 2);
			goItems[i].y1 = y;
			goItems[i].y2 = y + lineh + 10;
			goItems[i].bcolor = RGBA (64, 64, 64, 128);
			goItems[i].color = RGBA (192, 192, 192, 255);

			grlib_DrawButton (&goItems[i], BTNSTATE_NORM);
						
			y += lineh + YSPACING;
			}
		else
			{
			memset (&goItems[i], 0, sizeof (s_grlibobj));
			y += YSPACINGFAKE;
			}
			
		}
		
	grlib_PushScreen ();
	grlib_Render ();
	
	t = time(NULL) + grlibSettings.autoCloseMenu;
	do 
		{
		if (grlibSettings.autoCloseMenu > 0 && t < time(NULL))
			{item = MNUBTN_TOUT; break;}
		
		grlib_PopScreen ();
		
		// Check for menu
		item = -1;
		for (i = 0; i < itemsCnt; i++)
			{
			if (grlib_irPos.x > goItems[i].x1 && grlib_irPos.x < goItems[i].x2 && grlib_irPos.y > goItems[i].y1 && grlib_irPos.y < goItems[i].y2)
				{
				item = i;

				if (grlibSettings.theme.enabled)
					{
					s_grlibobj btn;
					
					grlib_MagObject (&btn, &goItems[i], 3, 3);
					grlib_DrawButton (&btn, BTNSTATE_SEL);
					}
				else
					{
					goItems[i].color = RGBA(255,255,255,255);
					grlib_DrawBoldEmptySquare (&goItems[i]);
					}
					
				break;
				}
			}
		
		if (grlibSettings.OverlayCallback != NULL) grlibSettings.OverlayCallback();
		grlib_DrawIRCursor ();
		grlib_Render ();

		btn = grlib_GetUserInput();
		if (btn & WPAD_BUTTON_A && item != -1) 
			break;
		else 
			{
			if (btn & WPAD_BUTTON_B) {item = MNUBTN_B; break;}
			if (btn & WPAD_BUTTON_PLUS) {item = MNUBTN_PLUS; break;}
			if (btn & WPAD_BUTTON_MINUS) {item = MNUBTN_MINUS; break;}
			}
			
		if (grlibSettings.wiiswitch_reset || grlibSettings.wiiswitch_poweroff)
			{
			item = -1;
			break;
			}
		} 
	while (TRUE);

	do {WPAD_ScanPads(); if (!WPAD_ButtonsDown(0)) break;} while (TRUE);
	/*
	if (grlibSettings.RedrawCallback != NULL) 
		{
		grlibSettings.RedrawCallback();
		grlib_PushScreen ();
		grlib_Render ();
		}
	*/	
	free (line);
	
	// Report cancel
	if (item < 0) return item;

	// Restore previous screen
	return retcodes[item];
	}
