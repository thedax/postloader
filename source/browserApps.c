#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <dirent.h>
#include <time.h>
#include "wiiload/wiiload.h"
#include "globals.h"
#include "bin2o.h"
#include "gui.h"
#include "mystring.h"
#include "devices.h"
#include "browser.h"
#include "mystring.h"

// 192x112
// 128x48
#define ICONW 128
#define ICONH 63

#define XMIDLEINFO 320
#define INIT_X (30 + ICONW / 2)
#define INIT_Y (60 + ICONH / 2)
#define INC_X ICONW+22
#define INC_Y ICONH+25

/*
This allow to browse for applications in apps folder of mounted drive
*/
#define AT_HBA 1
#define AT_FOLDER 2
#define AT_FOLDERUP 3 // This is to clear subfolder...

extern s_grlibSettings grlibSettings;

#define APPSMAX 256

static s_cfg *cfg;
static s_app *apps;	// cointains the current available items on filesistem.

static int appsCnt;
static int apps2Disp;
static int page; // Page to be displayed
static int pageMax; // 
static int appsSelected = -1;	// Current selected app with wimote
static int spotSelected = -1;
static int spotSelectedLast = -1;

static u8 redraw = 1;
static bool redrawIcons = true;

static u16 sortMode = 0;

static int browserRet = 0;
static int showHidden = 0;
static int needToSave = 0;
static int disableSpots;

static s_grlib_iconSetting is;

static void Redraw (void);

static void InitializeGui (void)
	{
	Debug ("InitializeGui %d", theme.ok);

	// Prepare black box
	int i, il;
	int x = INIT_X;
	int y = INIT_Y;

	gui_Init();

	gui.spotsIdx = 0;
	gui.spotsXpage = 16;
	gui.spotsXline = 4;

	grlib_IconSettingInit (&is);

	is.themed = theme.ok;
	is.border = 5.0;
	is.magX = 1.0;
	is.magY = 1.0;
	is.magXSel = 1.2;
	is.magYSel = 1.2;
	is.iconFake = vars.tex[TEX_NONE];
	is.bkgTex = theme.frameBack;
	is.fgrSelTex = theme.frameSel;
	is.fgrTex = theme.frame;
	is.bkgColor = RGBA (0,0,0,255);
	is.borderSelColor = RGBA (255,255,255,255); 	// Border color
	is.borderColor = RGBA (128,128,128,255); 		// Border color
	is.fontReverse = 0;

	il = 0;
	for (i = 0; i < gui.spotsXpage; i++)
		{
		grlib_IconInit (&gui.spots[i].ico, NULL);
		
		gui.spots[i].ico.x = x;
		gui.spots[i].ico.y = y;
		gui.spots[i].ico.w = ICONW;
		gui.spots[i].ico.h = ICONH;

		il++;
		if (il == gui.spotsXline)
			{
			x = INIT_X;
			y += INC_Y;
			il = 0;
			}
		else
			{
			x+=INC_X;
			}
		}
	}

static void FeedCoverCache (void)
	{
	char path[PATHMAX];

	CoverCache_Pause (true);

	int i;
	int ai;	// Application index (corrected by the offset)

	for (i = -gui.spotsXpage; i < gui.spotsXpage*2; i++)
		{
		ai = (page * gui.spotsXpage) + i;
		
		if (ai >= 0 && ai < appsCnt)
			{
			sprintf (path, "%s://apps/%s%s/icon.png", apps[ai].mount, config.subpath, apps[ai].path);
			CoverCache_Add (path,  (i >= 0 && i < gui.spotsXpage) ? true:false );
			}
		}
	
	CoverCache_Pause (false);
	}

static void AppsFree (void)
	{
	int i;

	Debug ("AppsFree");
	
	for (i = 0; i < appsCnt; i++)
		{
		if (apps[i].name != NULL) 
			{
			free (apps[i].name);
			apps[i].name = NULL;
			}
		if (apps[i].version != NULL) 
			{
			free (apps[i].version);
			apps[i].version = NULL;
			}
		if (apps[i].desc != NULL) 
			{
			free (apps[i].desc);
			apps[i].desc = NULL;
			}
		if (apps[i].path != NULL) 
			{
			free (apps[i].path);
			apps[i].path = NULL;
			}
		if (apps[i].args != NULL) 
			{
			free (apps[i].args);
			apps[i].args = NULL;
			}
		}
		
	appsCnt = 0;
	}

static void ParseXML (int ai)
	{
	char cfg[256];
	char *buff;

	sprintf (cfg, "%s://apps/%s/meta.xml", apps[ai].mount, apps[ai].path);
	buff = (char*)fsop_ReadFile (cfg, 0, NULL);

	if (!buff || strlen(buff) == 0) return;
	
	int l;
	int found;
	char folder[] = "(folder)";
	char *p1,*p2;
	char token[256];
	char args[1024];

	/////////////////////////////////////////////////
	// Now remove all comments from the buffer
	p1 = buff;
	do
		{
		found = 0;
		
		p1 = strstr (p1, "<!--");
		if (!p1) break;
		p2 = strstr (p1, "-->");
		if (!p2) break;
		
		if (p2 > p1)
			{
			found = 1;
			p2 += 3;
			while (p1 < p2)
				{
				*p1 = ' ';
				p1++;
				}
				
			p1 = p2;
			}
		}
	while (found);
	
	if (strstr (buff, "<no_ios_reload/>"))
		apps[ai].iosReload = 0;
	else
		apps[ai].iosReload = 1;

	/////////////////////////////////////////////////
	// scan for application name
	p1 = buff;
	strcpy (token, "<version>");
	p1 = strstr (p1, token);
	if (p1)
		{
		p2 = strstr (p1, "</version>");
		if (p2)
			{
			p1 += strlen(token);
			
			if (p2 > p1)
				{
				// we have the name
				l = p2-p1;
				apps[ai].version = malloc (l+1);
				strncpy (apps[ai].version, p1, l);
				apps[ai].version[l] = 0;
				}
			}
		}
		
	/////////////////////////////////////////////////
	// scan for application name
	p1 = buff;
	strcpy (token, "<name>");
	p1 = strstr (p1, token);
	if (p1)
		{
		p2 = strstr (p1, "</name>");
		if (p2)
			{
			p1 += strlen(token);
			
			if (p2 > p1)
				{
				// we have the name, check if start with space or lower chars
				while (*p1 <= 32 && p1 < p2) p1++;
				l = p2-p1;
				apps[ai].name = malloc (l+1);
				strncpy (apps[ai].name, p1, l);
				apps[ai].name[l] = 0;
				}
			}
		}
		
	/////////////////////////////////////////////////
	// scan for application description
	p1 = buff;
	strcpy (token, "<short_description>");
	p1 = strstr (p1, token);
	if (p1)
		{
		p2 = strstr (p1, "</short_description>");
		if (p2)
			{
			p1 += strlen(token);
			
			if (p2 > p1)
				{
				// we have the name
				while (*p1 <= 32 && p1 < p2) p1++;
				
				if (apps[ai].type == AT_FOLDER)
					{
					l = (p2-p1) + strlen(folder);
					if (l > 2047) l = 2047;
					apps[ai].desc = malloc (l + 64); // more space for cr/lf encoding
					strcpy (apps[ai].desc, folder);
					strcat (apps[ai].desc, " ");
					strncpy (apps[ai].desc, p1, l - strlen(folder));	
					apps[ai].desc[l] = 0;
					}
				else
					{
					l = p2-p1;
					if (l > 2047) l = 2047;
					apps[ai].desc = malloc (l + strlen(folder) + 64); // more space for cr/lf encoding
					strncpy (apps[ai].desc, p1, l);	
					apps[ai].desc[l] = 0;
					}
				}
			}
		}
	if (!apps[ai].desc && apps[ai].type == AT_FOLDER)
		{
		apps[ai].desc = calloc (strlen(folder) + 1,1);
		strcpy (apps[ai].desc, folder);
		}

	/////////////////////////////////////////////////	
	// scan for args
	args[0] = 0;
	p1 = buff;
	do
		{
		found = 0;
		
		strcpy (token, "<arg>");
		
		p1 = strstr (p1, token);
		if (!p1) break;
		
		p2 = strstr (p1, "</arg>");
		if (!p2) break;
		
		p1 += strlen(token);
		
		if (p2 > p1)
			{
			found = 1;
			strncat (args, p1, (int)(p2-p1));
			strcat (args, ";");
			}
		}
	while (found);
	
	if (strlen(args) > 0)
		{
		apps[ai].args = malloc (strlen(args)+1);
		strcpy (apps[ai].args, args);
		}
		
	if (apps[ai].name != NULL && *apps[ai].name == '\0')
		{
		free (apps[ai].name);
		apps[ai].name = NULL;
		}
		
	if (apps[ai].name == NULL)
		{
		apps[ai].name = ms_AllocCopy (apps[ai].path, 0);
		}
		
	free (buff);
	return;
	}

static char *GetLongdescFromXML (int ai)
	{
	char cfg[256];
	char *buff;
	char *longdesc = NULL;

	sprintf (cfg, "%s://apps/%s/meta.xml", apps[ai].mount, apps[ai].path);
	buff = (char*)fsop_ReadFile (cfg, 0, NULL);

	if (!buff || strlen(buff) == 0) return NULL;
	
	int l;
	char *p1,*p2;
	char token[256];

	/////////////////////////////////////////////////
	// get long_description
	p1 = buff;
	strcpy (token, "<long_description>");
	p1 = strstr (p1, token);
	if (p1)
		{
		p2 = strstr (p1, "</long_description>");
		if (p2)
			{
			p1 += strlen(token);
			
			if (p2 > p1)
				{
				// we have the name
				while (*p1 <= 32 && p1 < p2) p1++;
				l = p2-p1;
				if (l > 2048) l = 2048;
				longdesc = malloc (l + 64);  // more space for cr/lf encoding
				strncpy (longdesc, p1, l);
				longdesc[l] = 0;
				}
			}
		}

	return longdesc;
	}

static int AppsSetDefault (int ai)
	{
	config.autoboot.enabled = TRUE;
	config.autoboot.appMode = APPMODE_HBA;

	sprintf (config.autoboot.path,"%s:/apps/%s%s/", apps[ai].mount, config.subpath, apps[ai].path);
	sprintf (config.autoboot.filename, "%s", apps[ai].filename);
	
	if (apps[ai].args)
		strcpy (config.autoboot.args, apps[ai].args);
	else
		strcpy (config.autoboot.args, "");
		
	return 1;
	}

static int AppsSetRun (int ai)
	{
	config.run.appMode = APPMODE_HBA;
	
	sprintf (config.run.path,"%s:/apps/%s%s/", apps[ai].mount, config.subpath, apps[ai].path);
	sprintf (config.run.filename,"%s", apps[ai].filename);
	
	if (apps[ai].args)
		strcpy (config.run.args, apps[ai].args);
	else
		strcpy (config.run.args, "");
		
	config.run.enabled = 1;
	
	return 1;
	}

static int AppExist (const char *mount, char *path, char *filename)
	{
	char fn[256];
	char *xml, *dol, *elf;
	
	sprintf (fn, "%s://apps/%s%s", mount, config.subpath, path);
	Debug ("AppExist %s", fn);


	char *dir = fsop_GetDirAsString(fn,'|',1,".elf;.xml;.dol");
	if (!dir) return 0;
	
	elf = ms_strstr (dir, "boot.elf");
	xml = ms_strstr (dir, "meta.xml");
	dol = ms_strstr (dir, "boot.dol");
	
	free (dir);
	/*
	sprintf (fn, "%s://apps/%s%s/boot.elf", mount, config.subpath, path);
	elf = fsop_FileExist (fn);

	sprintf (fn, "%s://apps/%s%s/meta.xml", mount, config.subpath, path);
	xml = fsop_FileExist (fn);
	
	sprintf (fn, "%s://apps/%s%s/boot.dol", mount, config.subpath, path);
	dol = fsop_FileExist (fn);
	*/
	
	if (elf)
		strcpy (filename, "boot.elf");

	if (dol)
		strcpy (filename, "boot.dol");
	
	// Ok, we have classic hbapp
	if (dol || elf) return AT_HBA;
	
	// Ok, it seems to be a folder
	if (xml && !(dol || elf)) return AT_FOLDER;

	return 0;
	}

static bool IsFiltered (int ai)
	{
	int i;
	bool ret = false;
	
	if (config.appFilter == 0x0) return true;
	
	for (i = 0; i < APPCATS_MAX; i++)
		{
		if ((config.appFilter & (1 << i)) && (apps[ai].category & (1 << i)))
			{
			ret = true;
			}
		}
		
	return ret;
	}

static int bsort_name (const void * a, const void * b)
	{
	s_app *aa = (s_app*) a;
	s_app *bb = (s_app*) b;
	
	if (aa->type == AT_FOLDERUP) return 0;
	if (!aa->name || !bb->name) return 0;

	return (ms_strcmp (bb->name, aa->name) < 0 ? 1:0);
	}

static int bsort_hidden (const void * a, const void * b)
	{
	s_app *aa = (s_app*) a;
	s_app *bb = (s_app*) b;
	
	if (aa->type == AT_FOLDERUP) return 0;
	if (aa->hidden > bb->hidden) return 1;

	return 0;
	}

static int bsort_filtered (const void * a, const void * b)
	{
	s_app *aa = (s_app*) a;
	s_app *bb = (s_app*) b;
	
	if (aa->type == AT_FOLDERUP) return 0;
	if (aa->filtered < bb->filtered) return 1;

	return 0;
	}

static int bsort_priority (const void * a, const void * b)
	{
	s_app *aa = (s_app*) a;
	s_app *bb = (s_app*) b;
	
	if (aa->priority < bb->priority) return 1;
	
	return 0;
	}

static void SortItems (void)
	{
	int i;
	int filtered = 0;
	
	Debug ("AppSort (begin)");

	// Apply filters
	apps2Disp = 0;
	for (i = 0; i < appsCnt; i++)
		{
		apps[i].filtered = IsFiltered (i);
		if (apps[i].filtered && (!apps[i].hidden || showHidden)) apps2Disp++;
		if (apps[i].filtered) filtered++;
		}
	
	bsort (apps, appsCnt, sizeof(s_app), bsort_filtered);
	bsort (apps, filtered, sizeof(s_app), bsort_hidden);
	bsort (apps, apps2Disp, sizeof(s_app), bsort_name);
	bsort (apps, apps2Disp, sizeof(s_app), bsort_priority);

	pageMax = (apps2Disp - 1) / gui.spotsXpage;
	
	FeedCoverCache ();
	
	Debug ("AppSort (end)");
	}

static int ScanApps (const char *mount, int refresh)
	{
	DIR *pdir;
	struct dirent *pent;
	char path[PATHMAX];
	char filename[64];
	char buff[1024];		// uhhh large buffer
	char temp[4096];		// uhhh large buffer
	int apptype;

	sprintf (path, "%s://apps/%s", mount, config.subpath);
	Debug ("ScanApps: searching '%s'", path);
	
	pdir=opendir(path);
	cfg_Section (NULL);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if(strcmp(".", pent->d_name) == 0 || strcmp("..", pent->d_name) == 0)
	        continue;
			
		apptype = AppExist (mount, pent->d_name, filename);
		if (apptype)
			{
			apps[appsCnt].type = apptype;

			strcpy (apps[appsCnt].mount, mount); // Save the mount point
			
			apps[appsCnt].path = malloc (strlen (pent->d_name) + 1);
			strcpy (apps[appsCnt].path, pent->d_name);
			
			// Store the right filename
			if (apptype == AT_HBA)
				strcpy (apps[appsCnt].filename, filename);
			
			if (strlen(config.subpath) > 0)
				sprintf (path, "%s.%s%s", apps[appsCnt].mount, config.subpath, apps[appsCnt].path);
			else
				sprintf (path, "%s.%s", apps[appsCnt].mount, apps[appsCnt].path);

			*buff = 0;
			cfg_Value (cfg, CFG_READ, CFG_ENCSTRING, path, buff, -1);
			if (*buff && !refresh) // get from cache
				{
				cfg_FmtString (buff, CFG_READ, CFG_STRING, temp, 0);
				apps[appsCnt].name = ms_AllocCopy (temp, 0);
				cfg_FmtString (buff, CFG_READ, CFG_STRING, temp, 1);
				apps[appsCnt].version = ms_AllocCopy (temp, 0);
				cfg_FmtString (buff, CFG_READ, CFG_STRING, temp, 2);
				apps[appsCnt].args = ms_AllocCopy (temp, 0);
				cfg_FmtString (buff, CFG_READ, CFG_INT, &apps[appsCnt].priority, 3);
				cfg_FmtString (buff, CFG_READ, CFG_INT, &apps[appsCnt].iosReload, 4);
				cfg_FmtString (buff, CFG_READ, CFG_INT, &apps[appsCnt].hidden, 5);
				cfg_FmtString (buff, CFG_READ, CFG_U32, &apps[appsCnt].category, 6);
				cfg_FmtString (buff, CFG_READ, CFG_STRING, temp, 7);
				apps[appsCnt].desc = ms_AllocCopy (temp, 0);
				
				if (apps[appsCnt].name == NULL)
					apps[appsCnt].name = ms_AllocCopy (apps[appsCnt].path, 0);
				}
			else
				{
				ParseXML (appsCnt);
				
				Video_WaitPanel (TEX_HGL, "Please wait...|Searching for applications");

				needToSave = 1;
				}

			appsCnt++;
			if (appsCnt >= APPSMAX) break; // No more space

			//grlib_PopScreen ();
			//Video_DrawIcon (TEX_HGL, 320, 430);
			//grlib_Render ();
			}
		else
			Debug ("    > fails '%s'", pent->d_name);
		}

	closedir(pdir);
	
	Debug ("ScanApps completed!");
	
	return 0;
	}

static void UpdateAllSettings (s_cfg *cfg)
	{
	int i;
	char section[256];
	char buff[1024];		// uhhh large buffer
	
	Debug ("UpdateAllSettings: %d (0x%X)", appsCnt, cfg);
	
	cfg_Section (NULL);
	
	int ver = HBCFGVER;
	cfg_Value (cfg, CFG_WRITE, CFG_INT, "VERSION", &ver, -1);

	for (i = 0; i < appsCnt; i++)
		{
		if (apps[i].type != AT_FOLDERUP)
			{
			if (strlen(config.subpath) > 0)
				sprintf (section, "%s.%s%s", apps[i].mount, config.subpath, apps[i].path);
			else
				sprintf (section, "%s.%s", apps[i].mount, apps[i].path);

			*buff = '\0';
			cfg_FmtString (buff, CFG_WRITE, CFG_STRING, apps[i].name, 0);
			cfg_FmtString (buff, CFG_WRITE, CFG_STRING, apps[i].version, 1);
			cfg_FmtString (buff, CFG_WRITE, CFG_STRING, apps[i].args, 2);
			cfg_FmtString (buff, CFG_WRITE, CFG_INT, &apps[i].priority, 3);
			cfg_FmtString (buff, CFG_WRITE, CFG_INT, &apps[i].iosReload, 4);
			cfg_FmtString (buff, CFG_WRITE, CFG_INT, &apps[i].hidden, 5);
			cfg_FmtString (buff, CFG_WRITE, CFG_U32, &apps[i].category, 6);
			cfg_FmtString (buff, CFG_WRITE, CFG_STRING, apps[i].desc, 2);

			cfg_Value (cfg, CFG_WRITE, CFG_ENCSTRING, section, buff, -1);
			}
		}	
	Debug ("UpdateAllSettings: end");
	}
	
static void SaveSettings (void)
	{
	if (!needToSave) return;

	char path[PATHMAX];
	
	Video_WaitIcon (TEX_HGL);
	
	sprintf (path, "%s://ploader/homebrew.conf", vars.defMount);
	cfg = cfg_Alloc(path, 0, 0, 1);
	
	UpdateAllSettings (cfg);

	cfg_Store(cfg , path);
	cfg_Free(cfg );
	
	needToSave = 0;
	}

static int AppsBrowse (int refresh)
	{
	gui.spotsIdx = 0;

	Video_WaitIcon (TEX_HGL);
	
	char path[PATHMAX];
	sprintf (path, "%s://ploader/homebrew.conf", vars.defMount);
	
	cfg = cfg_Alloc(path, 0, 0, 1);
	
	int ver = 0;
	cfg_Value (cfg, CFG_READ, CFG_INT, "VERSION", &ver, -1);
	
	if (ver < HBCFGVER)
		{
		grlib_menu (50, "Information:\nCurrent homebrew database is invalid.\nA new one will be created.", "Ok");
		cfg_Empty (cfg);
		}
	
	if (needToSave)
		UpdateAllSettings (cfg);
	
	AppsFree ();
	gui_Clean();
	
	if (strlen(config.subpath) > 0)
		{
		apps[appsCnt].type = AT_FOLDERUP;
		apps[appsCnt].name = malloc (32);
		strcpy (apps[appsCnt].name, "Up to previous folder..."); 
		appsCnt++;
		
		// scan only selected folder (on sd or usb... its depend... :P)
		ScanApps (config.submount, refresh);
		}
	else
		{
		if ((config.appDev == 0 || config.appDev == 1) && devices_Get(DEV_SD))
			ScanApps (devices_Get(DEV_SD), refresh);

		if ((config.appDev == 0 || config.appDev == 2) && devices_Get(DEV_USB))
			ScanApps (devices_Get(DEV_USB), refresh);
		}
		
	if (needToSave)
		{
		UpdateAllSettings (cfg);
		cfg_Store(cfg , path);
		}
		
	cfg_Free (cfg);
		
	page = 0;
	needToSave = 0;
	
	SortItems ();
	
	return appsCnt;
	}

static char *GetFilterString (int maxwidth)
	{
	static u32 lastfilter = 0;
	static char string[256] = {0};
	int i;
	
	if (lastfilter == config.appFilter) // nothing changed
		return string;
		
	lastfilter = config.appFilter;
	
	*string = '\0';
	if (config.appFilter == 0x0 || config.appFilter == 0xFF)
		return string;
	
	for (i = 0; i < APPCATS_MAX; i++)
		if (config.appFilter & (1 << i))
			{
			strcat (string, config.appCats[i]);
			strcat (string, "/");
			}
			
	string[strlen(string)-1] = 0;
	
	// now limit width
	int cutted = 0;
	while (strlen(string) > 0 && grlib_GetFontMetrics(string, NULL, NULL) > maxwidth)
		{
		string[strlen(string)-1] = 0;
		cutted = 1;
		}
	if (cutted) strcat (string, "...");
	
	return string;
	}

static void ShowFilterMenu (void)
	{
	if (!CheckParental(0)) return;
	
	char buff[512];
	u8 f[APPCATS_MAX];
	int item;
	int i;
	
	spotSelected = -1;
	
	for (i = 0; i < APPCATS_MAX; i++)
		f[i] = (config.appFilter & (1 << i)) ? 1:0;

	do
		{
		*buff = '\0';

		for (i = 0; i < APPCATS_MAX; i++)
			{
			grlib_menuAddCheckItem (buff, 200+i, f[i], config.appCats[i]);
			}

		grlib_menuAddColumn (buff);

		grlib_menuAddCheckItem (buff, 100, config.appDev == 1 ? 1:0, "SD Device");
		grlib_menuAddCheckItem (buff, 101, config.appDev == 2 ? 1:0, "USB Device");
		grlib_menuAddCheckItem (buff, 102, config.appDev == 0 ? 1:0, "Both");
		
		item = grlib_menu (0, "Select filters and device(s):\nPress (B) to close. (+)/(-) Set/Clear all flags", buff);
		
		if (item == 100) config.appDev = 1;
		if (item == 101) config.appDev = 2;
		if (item == 102) config.appDev = 0;
		
		if (item == MNUBTN_PLUS)
			{
			for (i = 0; i < APPCATS_MAX; i++)
				f[i] = 1;
				
			needToSave = 1;
			}
			
		if (item == MNUBTN_MINUS)
			{
			for (i = 0; i < APPCATS_MAX; i++)
				f[i] = 0;
			
			needToSave = 1;
			}
			
		if (item >= 200)
			{
			int i = item - 200;
			f[i] = !f[i];
			
			needToSave = 1;
			}
		
		}
	while (item != MNUBTN_B);
	
	config.appFilter = 0;
	for (i = 0; i < APPCATS_MAX; i++)
		if (f[i]) config.appFilter |= (1 << i);
	
	if (needToSave) AppsBrowse (0);
	}

static void ShowRenameCatMenu (void)
	{
	if (!CheckParental(0)) return;
	
	int i, item;
	char buff[512];
	
	spotSelected = -1;
	
	do
		{
		*buff = '\0';

		for (i = 0; i < APPCATS_MAX; i++)
			{
			if (*config.appCats[i] == '\0') strcpy (config.appCats[i], "n/a");
			grlib_menuAddItem (buff, 200+i, config.appCats[i]);
			}

		item = grlib_menu (-200, "Rename categories\nPress (B) to close", buff);
		
		if (item >= 200)
			{
			int i = item - 200;
			grlib_Keyboard ("Enter new category name", config.appCats[i], 31);
			}
		
		}
	while (item >= 0);
	}

static void ShowChangeCatMenu (int ai)
	{
	if (!CheckParental(0)) return;
	
	char title[128];
	char buff[512];
	u8 f[APPCATS_MAX];
	int item;
	int i;
	
	for (i = 0; i < APPCATS_MAX; i++)
		f[i] = (apps[ai].category & (1 << i)) ? 1:0;

	do
		{
		*buff = '\0';

		for (i = 0; i < APPCATS_MAX; i++)
			{
			grlib_menuAddCheckItem (buff, 200+i, f[i], config.appCats[i]);
			}

		sprintf (title, "%s category(s):\nPress (B) to close", apps[ai].name);
		item = grlib_menu (0, title, buff);

		if (item >= 200)
			{
			int i = item - 200;
			f[i] = !f[i];
			}
		
		}
	while (item >= 0);
	
	apps[ai].category = 0;
	for (i = 0; i < APPCATS_MAX; i++)
		if (f[i]) apps[ai].category |= (1 << i);
	
	Debug ("%s -> %u", apps[ai].name, apps[ai].category);
	}

static void ShowAppMenu (int ai)
	{
	int len = 64;  // Give some space
	char *title;
	char buff[300];
	char *longdesc = GetLongdescFromXML(ai);
	
	Video_SetFont(TTFNORM);
	
	if (apps[ai].name) len += strlen(apps[ai].name);
	if (apps[ai].desc) len += strlen(apps[ai].desc);
	if (longdesc) len += strlen(longdesc);
	
	title = calloc (1, len);

	if (apps[ai].name)
		strcpy (title, apps[ai].name);
	else
		sprintf (title, "%s\n\nNo meta.xml found", apps[ai].path);
		
	if (apps[ai].desc) 
		{
		strcat (title, "\n");
		strncat (title, apps[ai].desc, 60);
		if (strlen(apps[ai].desc) > 60)
			strcat (title, "...");
		}
		
	if (longdesc)
		{
		int i, j, k, nl, lines;
		
		strcat (title, "\n\n");
		j = strlen (title);
		nl = 0;
		k = 0;
		lines = 0;
		for (i = 0; i < strlen (longdesc); i++)
			{
			if (k > 80) nl = 1;
			
			if (longdesc[i] == '\n')
				{
				k = 0;
				nl = 0;
				lines ++;
				}
			if (nl && longdesc[i] == ' ')
				{
				strcat (title, "\n");
				k = 0;
				nl = 0;
				lines ++;
				}
			else
				{
				title[j] = longdesc[i];
				title[j+1] = 0;			
				}
				
			if (lines > 12)
				{
				strcat (title, "... CUT ...");
				j = strlen(title);
				break;
				}
			j++;
			k++;
			}
		title[j] = 0;
		}

	*buff = '\0';

	if (CheckParental(0))
		{
		strcat (buff, "Set as autoboot application##0|");
		if (apps[ai].hidden)
			strcat (buff, "Remove hide flag##1");
		else
			strcat (buff, "Hide this application##2");
			
		strcat (buff, "~");
			
		strcat (buff, "Remove this application##5|");
		strcat (buff, "Change category##6");
		}
	strcat (buff, "Close##-1");
	
	spotSelected = -1;
	
	Video_SetFontMenu(TTFSMALL);
	int item = grlib_menu (0, title, buff);
	Video_SetFontMenu(TTFNORM);
	
	if (item == 0) AppsSetDefault (ai);
	
	if (item == 1)
		apps[ai].hidden = 0;

	if (item == 2)
		apps[ai].hidden = 1;

	if (item == 5)
		{
		char title[300];
		char buff[256];
		
		sprintf (buff,"%s:/apps/%s%s/", apps[ai].mount, config.subpath, apps[ai].path);
		sprintf (title, "Are you sure to kill this application ?\n\n%s", buff);
		if (grlib_menu (0, title, "Yes##1|No##0") == 1)
			{
			fsop_KillFolderTree (buff, NULL);
			}
		AppsBrowse (0);	
		item = -1;
		}
		
	if (item == 6)
		{
		ShowChangeCatMenu (ai);
		}
		
	free (title);
	
	if (item != -1) 
		{
		needToSave = 1;
		AppsBrowse (0);	
		}
		
	if (longdesc) free (longdesc);
	}

static void SortDispMenu (void)
	{
	char buff[300];
	
	*buff = '\0';
	
	grlib_menuAddItem (buff, 2, "Enter in interactive sort mode");
	grlib_menuAddItem (buff, 3, "Sort application by name");
	grlib_menuAddSeparator (buff);
	
	if (CheckParental(0))
		{
		if (showHidden)
			grlib_menuAddItem (buff, 4, "Hide hidden application");
		else
			grlib_menuAddItem (buff, 5, "Show hidden application");
		
		grlib_menuAddSeparator (buff);
		
		grlib_menuAddItem (buff, 6, "Rename categories");

		grlib_menuAddSeparator (buff);
		}
	strcat (buff, "Close##-1");
		
	spotSelected = -1;
	int item = grlib_menu (0, "Sort and display options...", buff);
	
	if (item == 6)
		{
		ShowRenameCatMenu ();
		}

	if (item == 4)
		{
		showHidden = 0;
		AppsBrowse (0);
		}
		
	if (item == 5)
		{
		showHidden = 1;
		AppsBrowse (0);
		}
		
	if (item == 2)
		{
		spotSelected = -1;
		int item = grlib_menu (0,
								"Sort mode:\n\n"
								"Plese click (A) on every application in the order\n"
								"you want them to be displayed.\n"
								"Click (B) to exit sort mode", 
								"OK|Cancel");
		if (item == 0) 
			sortMode = 1000; // Umph... maybe someone has more than 1000 apps ? 
		}
	if (item == 3)
		{
		spotSelected = -1;
		int item = grlib_menu (0,
								"Sort application by name:\n\n"
								"If you press OK, applications will\n"
								"be sorted by name",
								"OK|Cancel");
		if (item  == 0)
			{
			int i;
			for (i = 0; i < appsCnt; i++)
				{
				needToSave = 1;
				apps[i].priority = 1;
				}
			
			SortItems();
			}
		}
	}

static void ShowMainMenu (void)
	{
	int item;
	char buff[512];
	
	spotSelected = -1;
	
	do
		{
		buff[0] = '\0';
		grlib_menuAddItem (buff,  0, "Rescan devices");
		grlib_menuAddItem (buff,  1, "View options...");
		grlib_menuAddItem (buff,  2, "Select device(s)...");
		grlib_menuAddItem (buff,  3, "Refresh cache...");
			
		item = grlib_menu (0, "Homebrew menu", buff);
			
		if (item == 0)
			{
			MasterInterface (1, 0, 2, "Please wait...");
			MountDevices(0);
			AppsBrowse (0);
			}

		if (item == 1)
			{
			SortDispMenu ();
			}
		
		if (item == 3)
			{
			AppsBrowse (1); // let's refresh cache
			}
		
		if (CheckParental(0))
			{
			if (item == 2)
				{
				ShowFilterMenu ();
				}
			}
		}
	while (item >= 0);
	}

static int FindSpot (void)
	{
	int i;
	
	appsSelected = -1;
	spotSelected = -1;
	
	if (disableSpots) return appsSelected;
	
	for (i = 0; i < gui.spotsIdx; i++)
		{
		if (grlib_irPos.x > gui.spots[i].ico.rx1 && grlib_irPos.x < gui.spots[i].ico.rx2 && grlib_irPos.y > gui.spots[i].ico.ry1 && grlib_irPos.y < gui.spots[i].ico.ry2)
			{
			// Ok, we have the point
			appsSelected = gui.spots[i].id;
			spotSelected = i;
				
			break;
			}
		}
	
	if (spotSelectedLast != spotSelected)
		redraw = 1;
		
	spotSelectedLast = spotSelected;
	
	return appsSelected;
	}
	
void DrawInfo (void)
	{
	static time_t t = 0;

	if (appsSelected != -1)
		{
		char name[256];
		t = time(NULL);

		Video_SetFont(TTFNORM);
		
		*name = 0;
		if (apps[appsSelected].name)
			{
			if (apps[appsSelected].version && *apps[appsSelected].version != '\0')
				sprintf (name, "%s (%s)", apps[appsSelected].name, apps[appsSelected].version);
			else
				sprintf (name, "%s", apps[appsSelected].name);
			}

		grlib_printf (XMIDLEINFO, theme.line1Y, GRLIB_ALIGNCENTER, 0, name);

		Video_SetFont(TTFSMALL);

		if (apps[appsSelected].desc)
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, apps[appsSelected].desc);

		if (apps[appsSelected].type != AT_FOLDERUP && theme.line3Y > 0)
			{
			char rld[32];
			
			if (apps[appsSelected].iosReload)
				strcpy (rld, "");
			else
				strcpy (rld, " <no_ios_reload/>");
			
			if (apps[appsSelected].args != NULL && *apps[appsSelected].args != '\0')
				grlib_printf (XMIDLEINFO, theme.line3Y, GRLIB_ALIGNCENTER, 0, "/apps/%s%s (%s)%s",config.subpath, apps[appsSelected].path, apps[appsSelected].args, rld);
			else
				grlib_printf (XMIDLEINFO, theme.line3Y, GRLIB_ALIGNCENTER, 0, "/apps/%s%s%s",config.subpath, apps[appsSelected].path, rld);
			}
		}
	
	Video_SetFont(TTFSMALL);
	if (!grlib_irPos.valid)
		{
		if (appsSelected == -1) grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "Point an icon with the wiimote or use a GC controller!");		
		}
	else
		{
		if (time(NULL) - t > 0 && appsSelected == -1)
			{
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "(A) Execute (B) Settings (D-Pad) Switch mode (1) goto page");
			}
		}
	}
	
static void RedrawIcons (int xoff, int yoff)
	{
	int i;
	int ai;	// Application index (corrected by the offset)
	char path[128];

	Video_SetFont(TTFNORM);

	// Prepare black box
	for (i = 0; i < gui.spotsXpage; i++)
		{
		// Make sure that icon is not in sel state, and clean any ambiguity
		gui.spots[i].ico.sel = false;
		gui.spots[i].ico.title[0] = '\0';
		gui.spots[i].ico.iconOverlay[0] = NULL;
		gui.spots[i].ico.iconOverlay[1] = NULL;
		gui.spots[i].ico.iconOverlay[2] = NULL;
		gui.spots[i].ico.xoff = xoff;
		}
	
	gui.spotsIdx = 0;
	int spotIdx = -1;
	for (i = 0; i < gui.spotsXpage; i++)
		{
		ai = (page * gui.spotsXpage) + i;
		
		if (ai < appsCnt && ai < apps2Disp && gui.spotsIdx < SPOTSMAX)
			{
			sprintf (path, "%s://apps/%s%s/icon.png", apps[ai].mount, config.subpath, apps[ai].path);
			gui.spots[gui.spotsIdx].ico.icon = CoverCache_Get(path);

			gui.spots[gui.spotsIdx].ico.alticon = NULL;

			if (apps[ai].type == AT_FOLDERUP) 
				gui.spots[gui.spotsIdx].ico.alticon = vars.tex[TEX_FOLDERUP];

			if (apps[ai].type == AT_FOLDER) 
				gui.spots[gui.spotsIdx].ico.alticon = vars.tex[TEX_FOLDER];

			// Check if it is autoboot app
			if (config.autoboot.enabled && config.autoboot.appMode == APPMODE_HBA)
				{
				char path[300];
				sprintf (path,"%s:/apps/%s%s/", apps[ai].mount, config.subpath, apps[ai].path);
				if (strcmp (config.autoboot.path, path) == 0)
					gui.spots[gui.spotsIdx].ico.iconOverlay[0] = vars.tex[TEX_STAR];
				}

			// Is it hidden ?
			if (apps[ai].hidden && showHidden)
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = vars.tex[TEX_GHOST];
			else
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = NULL;
				
			if (apps[ai].checked)
				gui.spots[gui.spotsIdx].ico.iconOverlay[0] = vars.tex[TEX_CHECKED];
				
			// device icon
			if (apps[ai].mount[0] == 's' || apps[ai].mount[0] == 'S') 
				gui.spots[gui.spotsIdx].ico.iconOverlay[2] = vars.tex[TEX_SD];

			if (apps[ai].mount[0] == 'u' || apps[ai].mount[0] == 'U') 
				gui.spots[gui.spotsIdx].ico.iconOverlay[2] = vars.tex[TEX_USB];
			
			if (!gui.spots[gui.spotsIdx].ico.icon)
				{
				if (apps[ai].name)
					{
					char title[256];
					strcpy (title, apps[ai].name);
					title[48] = 0;
					strcpy (gui.spots[gui.spotsIdx].ico.title, title);
					}
				else
					strcpy (gui.spots[gui.spotsIdx].ico.title, apps[ai].path);
				}

			if (spotSelected == i) // Draw selected icon as last one
				{
				spotIdx = gui.spotsIdx;
				gui.spots[gui.spotsIdx].ico.sel = true;
				}
			else
				grlib_IconDraw (&is, &gui.spots[gui.spotsIdx].ico);
			
			gui.spots[gui.spotsIdx].id = ai;
			gui.spotsIdx++;
			}
		else
			{
			s_grlib_icon ico;
			grlib_IconInit (&ico, &gui.spots[i].ico);

			ico.noIcon = true;
			grlib_IconDraw (&is, &ico);
			}
		}
		
	if (spotIdx >= 0)
		{
		grlib_IconDraw (&is, &gui.spots[spotIdx].ico);
		}
	}

static void Redraw (void)
	{
	char sdev[64];
	
	Video_DrawBackgroud (1);
	
	*sdev = '\0';
	if (!config.subpath[0])
		{
		strcpy (sdev, "");
		
		if ((config.appDev == 0 || config.appDev == 1) && devices_Get(DEV_SD))
			{
			strcat (sdev, "[");
			strcat (sdev, devices_Get(DEV_SD));
			strcat (sdev, "] ");
			}

		if ((config.appDev == 0 || config.appDev == 2) && devices_Get(DEV_USB))
			{
			strcat (sdev, "[");
			strcat (sdev, devices_Get(DEV_USB));
			strcat (sdev, "] ");
			}
			
		if (strlen (sdev) == 0)
			{
			strcpy (sdev, "Invalid dev !!!");
			}
		}
	else
		{
		sprintf (sdev, "%s://apps/%s", config.submount, config.subpath);
		}
		
	Video_SetFont(TTFNORM);
	int w1 = grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader::HomeBrews - %s", sdev);
	int w2 = grlib_printf ( 615, 26, GRLIB_ALIGNRIGHT, 0, "Page %d of %d", page+1, pageMax+1);
	
	w1 = w1 + 25;
	w2 = 615 - w2;
	
	Video_SetFont(TTFVERYSMALL);
	grlib_printf ( w1 + (w2 - w1) / 2, 27, GRLIB_ALIGNCENTER, 0, GetFilterString((w2 - w1) - 30), sdev);

	if (redrawIcons) RedrawIcons (0,0);
	
	Video_DrawVersionInfo ();
	DrawInfo ();
	}
	
static void Overlay (void)
	{
	Video_DrawWIFI ();
	
	return;
	}
		
static int ChangePage (int next)
	{
	if (next)
		page++;
	else
		page--;
		
	if (page > pageMax)
		page = 0;
	if (page < 0)
		page = pageMax;
		
	FeedCoverCache ();

	redrawIcons = false;
	Redraw ();
	grlib_PushScreen ();
	
	int x = 0, lp;
	
	GRRLIB_SetFBMode (1); // Enable double fbmode
	
	if (!next)
		{
		do
			{
			x-=20;

			grlib_PopScreen ();

			lp = page;
			RedrawIcons (x + 640,0);
			if (page >= pageMax) page = 0; else page = lp+1;
			RedrawIcons (x,0);
			page = lp;
			
			Overlay ();
			grlib_DrawIRCursor ();
			grlib_Render();
			}
		while (x > -640);
		}
	else
		{
		do
			{
			x+=20;

			grlib_PopScreen ();

			lp = page;
			RedrawIcons (x - 640,0);
			if (page <= 0) page = pageMax; else page = lp-1;
			RedrawIcons (x,0);
			page = lp;
			
			Overlay ();
			grlib_DrawIRCursor ();
			grlib_Render();
			}
		while (x < 640);
		}
	
	redrawIcons = true;
	redraw = 1;
	
	GRRLIB_SetFBMode (0); // Enable double fbmode
	
	return page;
	}

static bool QuerySelection (int ai)
	{
	int i;
	float mag = 1.0;
	float magMax = 3.1;
	int spot = -1;
	int incX = 1, incY = 1;
	int y = 200;
	int yInf = 490;
	
	Video_SetFont(TTFNORM);
	
	for (i = 0; i < gui.spotsIdx; i++)
		{
		if (ai == gui.spots[i].id)
			spot = i;
		}
	
	if (spot < 0) return false;

	s_grlib_icon ico;
	grlib_IconInit (&ico, &gui.spots[spot].ico);
	ico.sel = true;
	
	s_grlib_iconSetting istemp;
	memcpy (&istemp, &is, sizeof(s_grlib_iconSetting));
	
	s_grlibobj black;
	black.x1 = 0;
	black.y1 = 0;
	black.x2 = grlib_GetScreenW();
	black.y2 = grlib_GetScreenH();
	black.color = RGBA(0,0,0,192);
	black.bcolor = RGBA(0,0,0,192);

	// Load full res cover
	char path[300];
	sprintf (path, "%s://apps/%s%s/icon.png", apps[ai].mount, config.subpath, apps[ai].path);
	GRRLIB_texImg * tex = GRRLIB_LoadTextureFromFile (path);
	if (tex) ico.icon = tex;
	
	while (true)
		{
		grlib_PopScreen ();
		grlib_DrawSquare (&black);

		istemp.magXSel = mag;
		istemp.magYSel = mag;
		
		incX = abs(ico.x - 320);
		if (incX > 10) incX = 10;

		incY = abs(ico.y - y);
		if (incY > 10) incY = 10;

		if (ico.x < 320) ico.x += incX;
		if (ico.x > 320) ico.x -= incX;

		if (ico.y < y) ico.y += incY;
		if (ico.y > y) ico.y -= incY;
		
		grlib_IconDraw (&istemp, &ico);

		DrawInfoWindo (yInf, apps[ai].name, "Press (A) to start, (B) Cancel");
		yInf -= 5;
		if (yInf < 400) yInf = 400;

		Overlay ();
		grlib_DrawIRCursor ();
		grlib_Render();
		
		if (mag < magMax) mag += 0.05;
		if (mag >= magMax && ico.x == 320 && ico.y == y) break;
		}
	
	u32 btn;
	while (true)
		{
		grlib_PopScreen ();
		grlib_DrawSquare (&black);
		grlib_IconDraw (&istemp, &ico);
		Overlay ();

		DrawInfoWindo (yInf, apps[ai].name, "Press (A) to start, (B) Cancel");
		yInf -= 5;
		if (yInf < 400) yInf = 400;
		
		grlib_DrawIRCursor ();
		grlib_Render();
		btn = grlib_GetUserInput();
		if (btn & WPAD_BUTTON_A) return true;
		if (btn & WPAD_BUTTON_B) return false;
		}
		
	GRRLIB_FreeTexture (tex);
	return true;
	}

int AppBrowser (void)
	{
	Debug ("AppBrowser");

	u32 btn;
	
	redraw = 1;
	appsSelected = -1;	// Current selected app with wimote
	spotSelected = -1;
	spotSelectedLast = -1;
	
	browserRet = -1;
	
	grlibSettings.color_window = RGBA(192,192,192,255);
	grlibSettings.color_windowBg = RGBA(32,32,32,128);
	
	grlib_SetRedrawCallback (Redraw, Overlay);
	
	apps = calloc (APPSMAX, sizeof(s_app));
	
	config.run.enabled = 0;
	
	// Immediately draw the screen...
	AppsFree ();
	
	InitializeGui ();
	
	/*
	Redraw ();
	grlib_PushScreen ();
	grlib_PopScreen ();
	grlib_Render();  // Render the frame buffer to the TV
	*/
	
	AppsBrowse (0);
	
	if (config.appPage >= 0 && config.appPage <= pageMax)
		page = config.appPage;
	else
		page = 0;

   FeedCoverCache ();
   LiveCheck (1);
   
   // Loop forever
    while (browserRet == -1)
		{
		if (LiveCheck (0)) redraw = 1;
		
		btn = grlib_GetUserInput();
		
		// If [HOME] was pressed on the first Wiimote, break out of the loop
		if (btn)
			{
			browserRet = ChooseDPadReturnMode (btn);
			if (browserRet != -1) break;
			
			if (btn & WPAD_BUTTON_1) 
				{
				spotSelected = -1;
				page = GoToPage (page, pageMax);
				FeedCoverCache ();
				redraw = 1;
				}
			if (btn & WPAD_BUTTON_2) 
				{
				ShowFilterMenu();
				redraw = 1;
				}
			if (btn & WPAD_BUTTON_A && appsSelected != -1 && sortMode == 0) 
				{
				if (apps[appsSelected].type == AT_HBA)
					{
					if (!QuerySelection (appsSelected))
						{
						redraw = 1;
						continue;
						}
					AppsSetRun (appsSelected);
					browserRet = INTERACTIVE_RET_HBSEL;
					break;
					}
				else if (apps[appsSelected].type == AT_FOLDER) // This is a folder ! Jump inside
					{	
					sprintf (config.subpath, "%s/", apps[appsSelected].path);
					sprintf (config.submount, "%s", apps[appsSelected].mount);
					
					AppsBrowse (0);
					redraw = 1;
					}
				else if (apps[appsSelected].type == AT_FOLDERUP) // This is a folder ! Jump inside
					{	
					int i = strlen(config.subpath);
					
					if (i > 0) i--;
					if (i > 0) i--;
					
					while (i >= 0 && config.subpath[i] != '/')
						config.subpath[i--] = 0;
					
					gui.spotsIdx = 0;
					AppsBrowse (0);
					redraw = 1;
					}
				}

			/////////////////////////////////////////////////////////////////////////////////////////////
			// We are in sort mode, check item
			if (btn & WPAD_BUTTON_A && appsSelected != -1 && sortMode > 0) 
				{
				if (!apps[appsSelected].checked)
					{
					apps[appsSelected].priority = sortMode--;
					apps[appsSelected].checked = TRUE;
					redraw = 1;
					//Debug ("selected %s (%d)\n", apps[appsSelected].name, apps[appsSelected].priority);
					}
				else
					{
					}
				}

			/////////////////////////////////////////////////////////////////////////////////////////////
			// Select application as default
			if (btn & WPAD_BUTTON_B && appsSelected != -1 && sortMode == 0)
				{
				ShowAppMenu (appsSelected);
				redraw = 1;
				}

			/////////////////////////////////////////////////////////////////////////////////////////////
			// If user press (B) stop sort mode
			if (btn & WPAD_BUTTON_B && sortMode > 0)
				{
				SortItems();
				
				int i;
				for (i = 0; i < appsCnt; i++)
					{
					apps[i].priority = appsCnt - i;
					apps[i].checked = FALSE;
					}
				sortMode = 0;
				needToSave = 1;
				redraw = 1;
				}

			if (btn & WPAD_BUTTON_HOME && sortMode > 0)
				{
				grlib_menu (0, "You are in sort mode.\n", "Close");
				redraw = 1;
				}

			if (btn & WPAD_BUTTON_HOME && sortMode == 0)
				{
				ShowMainMenu ();
				redraw = 1;
				}
			
			if (btn & WPAD_BUTTON_MINUS)
				{
				page = ChangePage (0);
				}
			if (btn & WPAD_BUTTON_PLUS) 
				{
				page = ChangePage (1);
				}
			}

		if (CoverCache_IsUpdated ()) 
			{
			redraw = 1;
			}
		
		FindSpot ();

		if (redraw)
			{
			Redraw ();
			grlib_PushScreen ();
			redraw = 0;
			}

		REDRAW();
		
		if (grlibSettings.wiiswitch_poweroff)
			{
			browserRet = INTERACTIVE_RET_SHUTDOWN;
			}

		if (grlibSettings.wiiswitch_reset)
			{
			browserRet = INTERACTIVE_RET_WIIMENU;
			}
			
		if (wiiload.status == WIILOAD_HBZREADY)
			{
			WiiloadZipMenu ();
			AppsBrowse (0);
			redraw = 1;
			}
			
		if (wiiload.status == WIILOAD_HBREADY)
			{
			if (WiiloadCheck())
				browserRet = INTERACTIVE_RET_WIILOAD;
			else
				redraw = 1;
			}
		
		if (vars.themeReloaded) // Restart the browser
			{
			vars.themeReloaded = 0;
			browserRet = INTERACTIVE_RET_TOHOMEBREW;
			}
		}
	
	// Lets close the topbar, if needed
	CLOSETOPBAR();
	CLOSEBOTTOMBAR();
	
	// save current page
	config.appPage = page;

	SaveSettings ();
	
	// Clean up all data
	AppsFree ();
	gui_Clean ();
	free (apps);
	
	grlib_SetRedrawCallback (NULL, NULL);

	return browserRet;
	}