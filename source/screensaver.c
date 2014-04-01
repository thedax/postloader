#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/video_types.h>
#include <dirent.h>
#include "globals.h"
#include "bin2o.h"
#include "devices.h"

static bool Play (char * fn) // return true interrupt the screensaver
	{
	static int xs = 0;
	GRRLIB_texImg *img;
	
	//Debug ("Play: %s", fn);
	
	if (fn == NULL)
		img = GRRLIB_LoadTexturePNG (icon_png);
	else
		{
		//mt_Lock();
		img = GRRLIB_LoadTextureFromFile (fn);
		//mt_Unlock();
		}

	if (!img) return false;
	
	int x = (rand () % 500) + 70;
	int y = (rand () % 300) + 90;

	GRRLIB_Rectangle (0, 0, 640, 480, RGBA (0,0,0,255), 1 );
	grlib_PushScreen ();
	
	int i;
	float z = 0;
	
	float ratio = ((float)img->w / (float)img->h)/1.3;
	
	Video_SetFont(TTFNORM);

	for (i = 360; i >= 0; i-=4)
		{
		grlib_PopScreen ();
		grlib_DrawImgCenter (x, y, z, z/ratio, img, (float)i, RGBA(255,255,255,255));
		
		grlib_printf (xs++, 450, GRLIB_ALIGNCENTER, 0, "postLoader"VER" (press any key)");
		if (xs > 1000) xs = -360;
		
		grlib_Render ();
		usleep (1000);
		
		if (grlib_GetUserInput() || grlibSettings.wiiswitch_reset || grlibSettings.wiiswitch_poweroff) return true;
		
		if (fn == NULL)
			z += 5;
		else
			z += 3;
		}
	
	for (i = 0; i < 100; i++)
		{
		grlib_PopScreen ();
		grlib_DrawImgCenter (x, y, z, z/ratio, img, 0, RGBA(255,255,255,255));
		
		grlib_printf (xs++, 450, GRLIB_ALIGNCENTER, 0, "postLoader"VER" (press any key)");
		if (xs > 1000) xs = -360;
		
		grlib_Render ();
		usleep (1000);
		
		if (grlib_GetUserInput() || grlibSettings.wiiswitch_reset || grlibSettings.wiiswitch_poweroff) return true;
		}

	for (i = 255; i >= 0; i -= 5)
		{
		grlib_PopScreen ();
		grlib_DrawImgCenter (x, y, z, z/ratio, img, 0, RGBA(255,255,255,i));
		
		grlib_printf (xs++, 450, GRLIB_ALIGNCENTER, 0, "postLoader"VER" (press any key)");
		if (xs > 1000) xs = -360;
		
		grlib_Render ();
		usleep (1000);
		
		if (grlib_GetUserInput() || grlibSettings.wiiswitch_reset || grlibSettings.wiiswitch_poweroff) return true;
		}

	GRRLIB_FreeTexture (img);
	
	devices_TickUSB ();
	
	snd_Mp3Go ();
	
	return false;
	}

#define PATHS 2
static bool ScreenSaver (void)
	{
	bool ret = false;
	DIR *pdir[2];
	struct dirent *pent;
	int fr;
	int cp = 0;
	int count = 0;

	char path[2][128];
	char fn[128];
	
	Debug ("ScreenSaver: start"); 
	
	fr = grlibSettings.fontDef.reverse;
	grlibSettings.fontDef.reverse = 0;
	
	sprintf (path[0], "%s://ploader/covers", vars.defMount);
	sprintf (path[1], "%s://ploader/covers.emu", vars.defMount);
	
	pdir[0] = NULL;
	pdir[1] = NULL;
		
	do
		{
		if (!pdir[cp])
			pdir[cp] = opendir(path[cp]);
			
		if (pdir[cp])
			pent = readdir(pdir[cp]);
		else
			pent = NULL;
		
		if (pent)
			{
			if (strstr (pent->d_name, ".png") != NULL || strstr (pent->d_name, ".PNG") != NULL)
				{
				sprintf (fn, "%s/%s", path[cp], pent->d_name);
				
				Debug ("ScreenSaver: %s", fn);
				
				ret = Play (fn);
				if (ret) break;
				}
			}
			
		if (count++ > 4)
			{
			ret = Play (NULL);
			if (ret) break;
			}
			
		if (!pent)
			{
			if (pdir[cp]) closedir(pdir[cp]);
			pdir[cp] = NULL;
			cp ++;
			if (cp >= PATHS) cp = 0;
			}
		}
	while (!ret);
	
	if (pdir[0]) closedir(pdir[0]);
	if (pdir[1]) closedir(pdir[1]);
	
	grlibSettings.fontDef.reverse = fr;
	
	if (ret) return true;
	return false;
	}
	
// Do burnin checking and hdd live and mp3 playing
bool LiveCheck (int reset)
	{
	static int ss = 0;
	static time_t t = 0;
	static time_t tss = 0;
	static time_t tp = 0;
	static time_t ct = 0; // current time
	static int xLast = 0, yLast = 0, bAct = 0;
	
	ct = time(NULL);
	
	if (reset)
		{
		tp = tss = t = ct;
		}
		
	// No user input ?
	if (grlib_irPos.x == xLast && grlib_irPos.y == yLast && grlibSettings.buttonActivity == bAct)
		{
		if ((ct - tss) > 1)
			{
			tss = ct;
			ss ++;
			}
			
		if (ss > 60)
			return ScreenSaver ();
		}
	else
		{
		xLast = grlib_irPos.x;
		yLast = grlib_irPos.y;
		bAct = grlibSettings.buttonActivity;
		ss = 0;
		}
	
	if ((ct - t) > 30)
		{
		t = ct;
		
		devices_TickUSB ();
		}

	if ((ct - tp) > 2)
		{
		tp = ct;
		snd_Mp3Go ();
		}
	
	return false;
	}