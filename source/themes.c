/*
Themes selection menu
*/

#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <wiiuse/wpad.h>
#include <dirent.h>

#include "wiiload/wiiload.h"
#include "globals.h"
#include "devices.h"

#define MAXTHEMES 24
#define MAXROW 8

int ThemeSelect (void)
	{
	int i, j;
	DIR *pdir;
	struct dirent *pent;
	char path[128], targpath[128];
	char menu[2048];
	char files[MAXTHEMES][64];
	char buff[64];
	char *p;
	
	sprintf (path, "%s://ploader/themes", vars.defMount);
	
	*menu = '\0';
	i = 0;
	j = 0;
	
	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if (strstr (pent->d_name, ".zip"))
			{
			strcpy (buff, pent->d_name);
			strcpy (files[i], buff);
			
			p = strstr (buff, ".");
			if (p) *p = '\0';
			grlib_menuAddItem (menu, i, buff);
			
			i++;
			j++;
		
			if (j == MAXROW)
				{
				grlib_menuAddColumn (menu);
				j = 0;
				}
			
			if (i == MAXTHEMES)
				{
				break;
				}
			}
		}
		
	closedir(pdir);

	if (i == 0)
		{
		grlib_menu ("No themes found in ploader/themes folder", "OK");
		return 0;
		}
	
	int ret = grlib_menu ("Please select a theme file\n\nPress (B) to close", menu);
	if (ret < 0) return 0;
		
	sprintf (targpath, "%s://ploader/theme", vars.defMount);
	
	fsop_KillFolderTree (targpath, NULL);
	fsop_MakeFolder (targpath);
	
	strcat (path, "/");
	strcat (path, files[ret]);
	
	ZipUnpack (path, targpath, NULL, NULL);
	
	sprintf (targpath, "%s://ploader/theme/ploader.png", vars.defMount);
	if (fsop_FileExist (targpath) && devices_Get (DEV_SD))
		{
		i = grlib_menu ("This theme contain a splash screen\n\nDo you want to install it ?", "Yes##1~No##-1~Remove##0");
		if (i != -1)
			unlink ("sd://ploader.png");
		if (i == 1)
			{
			if (!fsop_CopyFile (targpath, "sd://ploader.png", NULL))
				{
				grlib_menu ("An error occured copying the splash...", "   OK   ");
				}
			}
		}
	
	Video_LoadTheme (0);
	Video_LoadTheme (1);
	
	vars.themeReloaded = 1;
	
	return 1;
	}