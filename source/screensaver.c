#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/video_types.h>
#include <dirent.h>
#include "globals.h"

static bool Play (char * fn) // return true interrupt
	{
	GRRLIB_texImg *img;

	img = GRRLIB_LoadTextureFromFile (fn);
	
	//grlib_dosm ("%s: 0x%X", fn, img);

	if (!img) return;
	
	int x = (rand () % 500) + 70;
	int y = (rand () % 300) + 90;

	int xt = (rand () % 400) + 140;
	int yt = (rand () % 400) + 40;

	GRRLIB_Rectangle (0, 0, 640, 480, RGBA (0,0,0,255), 1 );
	grlib_printf (xt,yt-5, GRLIB_ALIGNCENTER, 0, "postLoader"VER" (press any key)");
	grlib_PushScreen ();
	
	//x = 320;
	//y = 200;
	
	int i;
	float z = 0;
	
	float ratio = ((float)img->w / (float)img->h)/1.3;
	
	grlib_SetFontBMF (fonts[FNTNORM]);

	for (i = 360; i >= 0; i-=4)
		{
		//grlib_dosm ("%d", i);
		grlib_PopScreen ();
		grlib_DrawImgCenter (x, y, z, z/ratio, img, (float)i, RGBA(255,255,255,255));
		
		grlib_Render ();
		usleep (1000);
		
		if (grlib_GetUserInput()) return true;
		
		z++;
		}
	usleep (1000 * 1000);

	GRRLIB_FreeTexture (img);
	
	return false;
	}

static bool ScreenSaver (void)
	{
	bool ret;
	DIR *pdir;
	struct dirent *pent;
	int fr;

	char path[128];
	char fn[128];
	
	fr = grlibSettings.fontBMF_reverse;
	grlibSettings.fontBMF_reverse = 0;
	
	sprintf (path, "%s://ploader/covers", vars.mount[DEV_SD]);
		
	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if (strstr (pent->d_name, ".png") != NULL || strstr (pent->d_name, ".PNG") != NULL)
			{
			sprintf (fn, "%s/%s", path, pent->d_name);
			
			ret = Play (fn);
			if (ret) break;
			}
		}
		
	closedir(pdir);
	
	grlibSettings.fontBMF_reverse = fr;
	
	if (ret) return true;
	return false;
	}
	
// Do burnin checking and hdd live
bool LiveCheck (int reset)
	{
	static int ss = 0;
	static time_t t = 0;
	static time_t tss = 0;
	
	static int xLast = 0, yLast = 0, bAct = 0;
	
	if (reset)
		{
		tss = t = time(NULL);
		}
		
	// No user input ?
	if (grlib_irPos.x == xLast && grlib_irPos.y == yLast && grlibSettings.buttonActivity == bAct)
		{
		if ((time(NULL) - tss) > 1)
			{
			tss = time(NULL);
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
		
	if ((time(NULL) - t) > 30 && IsDevValid(DEV_USB))
		{
		t = time(NULL);
		
		FILE *f;
		char path[PATHMAX];
		sprintf (path, "%s://pllive.dat", vars.mount[DEV_USB]);
		f = fopen (path, "wb");
		fwrite (f, 1, sizeof(FILE), f);
		fclose (f);
		
		unlink (path);
		}
	
	return false;
	}