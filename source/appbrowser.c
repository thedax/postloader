#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <dirent.h>
#include <time.h>
#include "wiiload/wiiload.h"
#include "globals.h"
#include "bin2o.h"
#include "gui.h"

// 192x112
// 128x48
#define ICONW 128
#define ICONH 63

#define XMIDLEINFO 320
#define INIT_X (30 + ICONW / 2)
#define INIT_Y (60 + ICONH / 2)
#define INC_X ICONW+22
#define INC_Y ICONH+25

#define AT_HBA 1
#define AT_FOLDER 2
#define AT_FOLDERUP 3 // This is to clear subfolder...

/*

This allow to browse for applications in apps folder of mounted drive




*/
extern s_grlibSettings grlibSettings;

#define APPSMAX 256

static s_app *apps;
static int appsCnt;
static int apps2Disp;
static int appsPage; // Page to be displayed
static int appsPageMax; // 
static int appsSelected = -1;	// Current selected app with wimote
static int spotSelected = -1;
static int spotSelectedLast = -1;

static u8 redraw = 1;


static char subpath[64];
static char submount[6];

static u16 sortMode = 0;

static int browserRet = 0;
static int showHidden = 0;

static s_grlib_iconSetting is;

static void Redraw (void);
static int AppsManageCfgFile (int ia, int write);

static void InitializeGui (void)
	{
	Debug ("InitializeGui");

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
	is.bkgMsk = theme.frameMask;
	is.bkgTex = theme.frameBack;
	is.fgrSelTex = theme.frameSel;
	is.fgrTex = theme.frame;
	is.bkgColor = RGBA (0,0,0,255);
	is.borderSelColor = RGBA (255,255,255,255); 	// Border color
	is.borderColor = RGBA (128,128,128,255); 		// Border color
	is.fontReverse = 0; 		// Border color

	il = 0;
	for (i = 0; i < gui.spotsXpage; i++)
		{
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
		if (apps[i].desc != NULL) 
			{
			free (apps[i].desc);
			apps[i].desc = NULL;
			}
		if (apps[i].longdesc != NULL) 
			{
			free (apps[i].longdesc);
			apps[i].longdesc = NULL;
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

static void SaveSettings (void)
	{
	int i;

	for (i = 0; i < appsCnt; i++)
		{
		if (apps[i].needUpdate)
			{
			AppsManageCfgFile (i, 1);
			}
		}
	}

static char * MallocReadMetaXML (char *fn, int morebytes)
	{
	char *buff = NULL;
	int size;
	FILE* f = NULL;
	
	f = fopen(fn, "rb");
	if (!f) return NULL;

	//Get file size
	fseek( f, 0, SEEK_END);
	size = ftell(f);
	
	if (size <= 0)
		{
		fclose (f);
		return NULL;
		}
	fseek( f, 0, SEEK_SET);
	
	buff = calloc (size+morebytes+1, 1);
	if (!buff)
		{
		fclose (f);
		return NULL;
		}
	fread (buff, size, 1, f);
	fclose (f);
	
	return buff;
	}

static void ParseXML (char * buff, int ai)
	{
	int found;
	char *p1,*p2;
	char token[256];
	char args[1024];
	char folder[] = "(folder)";

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
				// we have the name
				apps[ai].name = calloc (p2-p1+1,1);
				strncpy (apps[ai].name, p1, p2-p1);
				}
			}
		}
		
	/////////////////////////////////////////////////
	// scan for application name
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
				apps[ai].desc = calloc (p2-p1 + strlen(folder) + 2,1);
				
				if (apps[ai].type == AT_FOLDER)
					{
					strcpy (apps[ai].desc, folder);
					strcat (apps[ai].desc, " ");
					strncat (apps[ai].desc, p1, p2-p1);	
					}
				else
					strncpy (apps[ai].desc, p1, p2-p1);	
				}
			}
		}
	if (!apps[ai].desc && apps[ai].type == AT_FOLDER)
		{
		apps[ai].desc = calloc (strlen(folder) + 1,1);
		strcpy (apps[ai].desc, folder);
		}
	
	/////////////////////////////////////////////////
	// scan for application name
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
				apps[ai].longdesc = calloc (p2-p1+1,1);
				strncpy (apps[ai].longdesc, p1, p2-p1);
				}
			}
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
	return;
	}

static int AppsManageCfgFile (int ia, int write)
	{
	int ret = 0;
	char cfg[256];
	char token[256];
	char *buff;
	char *p;
	FILE* f = NULL;
	
	sprintf (cfg, "%s://apps/%s%s/meta.xml", apps[ia].mount, subpath, apps[ia].path);
	
	buff = MallocReadMetaXML (cfg, 256);
	
	if (buff == NULL) // Maybe meta is not present, anyway allocate it, as we need to store postloader cfg
		buff = calloc (256, 1);
	
	if (write)
		{
		f = fopen(cfg, "wb");
		if (!f) return 0;
		
		// remove old setting
		strcpy (token, "</app>");
		p = strstr (buff, token);
		
		if (p != NULL)
			{
			p += strlen(token);
			*p = 0;
			}
			
		sprintf (token, "\n<ploader>%d;%d;%d</ploader>", apps[ia].priority, apps[ia].hidden, apps[ia].fixCrashOnExit);
		strcat (buff, token);

		ret = fwrite(buff, 1, strlen(buff), f );
		fclose(f);
		}
	else
		{
		int err = 0;
		
		ParseXML (buff, appsCnt);
		
		// Search for config
		strcpy (token, "<ploader>");
		p = strstr (buff, token);
		
		if (p)
			{
			p += strlen(token);
			apps[ia].priority = atoi(p);
			}
		else
			err = 1;

		if (p)
			{
			p = strstr (p, ";");
			if (p)
				{
				p++;
				apps[ia].hidden = atoi(p);
				}
			else
				err = 1;
			}
			
		if (p)
			{
			p = strstr (p, ";");
			if (p)
				{
				p++;
				apps[ia].fixCrashOnExit = atoi(p);
				}
			else
				err = 1;
			}
			
		if (err)
			{
			apps[ia].priority = 1;
			apps[ia].hidden = 0;
			apps[ia].fixCrashOnExit = 0;
			}
		}
	
	free (buff);

	apps[ia].needUpdate = FALSE;

		
	return ret;
	}

static int AppsSetDefault (int ia)
	{
	config.autoboot.enabled = TRUE;
	config.autoboot.appMode = APPMODE_HBA;
	config.autoboot.fixCrashOnExit = apps[ia].fixCrashOnExit;
	sprintf (config.autoboot.path,"%s:/apps/%s%s/", apps[ia].mount, subpath, apps[ia].path);
	sprintf (config.autoboot.filename, "%s", apps[ia].filename);
	
	if (apps[ia].args)
		strcpy (config.autoboot.args, apps[ia].args);
	else
		strcpy (config.autoboot.args, "");
		
	// Update setup
	ConfigWrite ();
	
	return 1;
	}

static int AppsSetRun (int ia)
	{
	config.run.appMode = APPMODE_HBA;
	config.run.fixCrashOnExit = apps[ia].fixCrashOnExit;
	
	sprintf (config.run.path,"%s:/apps/%s%s/", apps[ia].mount, subpath, apps[ia].path);
	sprintf (config.run.filename,"%s", apps[ia].filename);
	
	if (apps[ia].args)
		strcpy (config.run.args, apps[ia].args);
	else
		strcpy (config.run.args, "");
		
	// Update setup
	ConfigWrite ();
	
	config.run.enabled = 1;
	
	return 1;
	}

static int AppExist (const char *mount, char *path, char *filename)
	{
	char fn[256];
	bool xml, dol, elf;
	
	sprintf (fn, "%s://apps/%s%s/boot.elf", mount, subpath, path);
	elf = fsop_FileExist (fn);
	
	sprintf (fn, "%s://apps/%s%s/meta.xml", mount, subpath, path);
	xml = fsop_FileExist (fn);
	
	sprintf (fn, "%s://apps/%s%s/boot.dol", mount, subpath, path);
	dol = fsop_FileExist (fn);
	
	if (elf)
		strcpy (filename, "boot.elf");

	if (dol)
		strcpy (filename, "boot.dol");
	
	// Ok, we have classic hbapp
	if (dol || elf) return AT_HBA;
	
	// Ok, it seems to be a folder
	if (xml & !dol) return AT_FOLDER;

	return 0;
	}

static void AppsSort (void)
	{
	int i;
	int mooved;
	s_app app;

	// Apply filters
	apps2Disp = 0;
	for (i = 0; i < appsCnt; i++)
		{
		if ((!apps[i].hidden || showHidden)) apps2Disp++;
		}
	
	// Sort by name, fucking ass stupid algorithm...
	do
		{
		mooved = 0;
		
		for (i = 0; i < appsCnt - 1; i++)
			{
			if (apps[i].hidden > apps[i+1].hidden && apps[i].type != AT_FOLDERUP)
				{
				// swap
				memcpy (&app, &apps[i+1], sizeof(s_app));
				memcpy (&apps[i+1], &apps[i], sizeof(s_app));
				memcpy (&apps[i], &app, sizeof(s_app));
				mooved = 1;
				}
			}
		}
	while (mooved);

	// Sort by name, fucking ass stupid algorithm...
	do
		{
		mooved = 0;
		
		for (i = 0; i < apps2Disp - 1; i++)
			{
			if (apps[i].name && apps[i+1].name && strcmp (apps[i+1].name, apps[i].name) < 0 && apps[i].type != AT_FOLDERUP)
				{
				// swap
				memcpy (&app, &apps[i+1], sizeof(s_app));
				memcpy (&apps[i+1], &apps[i], sizeof(s_app));
				memcpy (&apps[i], &app, sizeof(s_app));
				mooved = 1;
				}
			}
		}
	while (mooved);

	// Sort by priority, fucking ass stupid algorithm...
	do
		{
		mooved = 0;
		
		for (i = 0; i < apps2Disp - 1; i++)
			{
			if (apps[i+1].priority > apps[i].priority && apps[i].type != AT_FOLDERUP)
				{
				// swap
				memcpy (&app, &apps[i+1], sizeof(s_app));
				memcpy (&apps[i+1], &apps[i], sizeof(s_app));
				memcpy (&apps[i], &app, sizeof(s_app));
				mooved = 1;
				}
			}
		}
	while (mooved);
	}

static int ScanApps (const char *mount)
	{
	DIR *pdir;
	struct dirent *pent;
	char path[PATHMAX];
	char filename[32];
	int apptype;

	sprintf (path, "%s://apps/%s", mount, subpath);
	
	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if(strcmp(".", pent->d_name) == 0 || strcmp("..", pent->d_name) == 0)
	        continue;
		
		apptype = AppExist(mount, pent->d_name, filename);	    
		if (apptype)
			{
			strcpy (apps[appsCnt].mount, mount); // Save the mount point
			
			apps[appsCnt].type = apptype;
			apps[appsCnt].path = malloc (strlen (pent->d_name) + 1);
			
			strcpy (apps[appsCnt].path, pent->d_name);
			
			// Store the right filename
			if (apptype == AT_HBA)
				strcpy (apps[appsCnt].filename, filename);
			
			AppsManageCfgFile (appsCnt, 0);

			appsCnt++;
			if (appsCnt >= APPSMAX) break; // No more space

			grlib_PopScreen ();
			Video_DrawIcon (TEX_HGL, 320, 430);
			grlib_Render ();
			}
		}

	closedir(pdir);
	
	return 0;
	}

static int AppsBrowse (void)
	{
	int i;

	gui.spotsIdx = 0;
	AppsFree ();
	
	gui_Clean();
	
	//Video_WaitPanel (TEX_HGL, "Updating homebrew...", appsCnt);

	// We are in a subfolder... so add folderup icon
	if (strlen(subpath) > 0)
		{
		apps[appsCnt].type = AT_FOLDERUP;
		apps[appsCnt].name = malloc (32);
		strcpy (apps[appsCnt].name, "Up to previous folder..."); 
		appsCnt++;
		
		// scan only selected folder (on sd or usb... its depend... :P)
		ScanApps (submount);
		}
	else
		{
		for (i = 0; i < DEVMAX; i++)
			{
			if (vars.mount[i][0]) ScanApps (vars.mount[i]);
			}
		}
		
	
	appsPage = 0;
	appsPageMax = appsCnt / gui.spotsXpage;
	
	AppsSort ();
	
	return appsCnt;
	}

static GRRLIB_texImg * GetTexture (int ai)
	{
	char path[PATHMAX];
	
	sprintf (path, "%s://apps/%s%s/icon.png", apps[ai].mount, subpath, apps[ai].path);
	return GRRLIB_LoadTextureFromFile (path);
	}
		
static void ShowAppMenu (int ai)
	{
	int len = 64;  // Give some space
	char *title;
	char buff[300];
	
	grlib_SetFontBMF(fonts[FNTNORM]);
	
	if (apps[ai].name) len += strlen(apps[ai].name);
	if (apps[ai].desc) len += strlen(apps[ai].desc);
	if (apps[ai].longdesc) len += strlen(apps[ai].longdesc);
	
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
		
	if (apps[ai].longdesc)
		{
		int i, j, k, nl, lines;
		
		strcat (title, "\n\n");
		j = strlen (title);
		nl = 0;
		k = 0;
		lines = 0;
		for (i = 0; i < strlen (apps[ai].longdesc); i++)
			{
			if (k > 50) nl = 1;
			
			if (apps[ai].longdesc[i] == '\n')
				{
				k = 0;
				nl = 0;
				lines ++;
				}
			if (nl && apps[ai].longdesc[i] == ' ')
				{
				strcat (title, "\n");
				k = 0;
				nl = 0;
				lines ++;
				}
			else
				{
				title[j] = apps[ai].longdesc[i];
				title[j+1] = 0;			
				}
				
			if (lines > 15)
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

	buff[0] = '\0';
	strcat (buff, "Set as autoboot application##0|");
	if (apps[ai].hidden)
		strcat (buff, "Remove hide flag##1|");
	else
		strcat (buff, "Hide this application##2|");
		
	if (apps[ai].fixCrashOnExit)
		strcat (buff, "Fix crash on exit (active)##3|");
	else
		strcat (buff, "Fix crash on exit (disabled)##4|");
	
	strcat (buff, "Remove this application##5");
	strcat (buff, "Close##-1");

	int item = grlib_menu (title, buff);
	
	if (item == 0) AppsSetDefault (ai);
	
	if (item == 1)
		apps[ai].hidden = 0;

	if (item == 2)
		apps[ai].hidden = 1;

	if (item == 3)
		apps[ai].fixCrashOnExit = 0;

	if (item == 4)
		apps[ai].fixCrashOnExit = 1;
		
	if (item == 5)
		{
		//xxxxxx
		char title[300];
		char buff[256];
		
		sprintf (buff,"%s:/apps/%s%s/", apps[ai].mount, subpath, apps[ai].path);
		sprintf (title, "Are you sure to kill this application ?\n\n%s", buff);
		if (grlib_menu (title, "Yes##1|No##0") == 1)
			{
			fsop_KillFolderTree (buff, NULL);
			}
		SaveSettings ();
		AppsBrowse ();	
		item = -1;
		}
		
	free (title);
	
	if (item != -1) 
		{
		apps[ai].needUpdate = TRUE;
		SaveSettings ();
		AppsBrowse ();	
		}
	}

static void SortDispMenu (void)
	{
	char buff[300];
	
	strcpy (buff, "");
	strcat (buff, "Enter in interactive sort mode##2|");
	strcat (buff, "Sort application by name##3|");
	strcat (buff, "|");
	if (showHidden)
		strcat (buff, "Hide hidden application##4|");
	else
		strcat (buff, "Show hidden application##4|");
	strcat (buff, "|");
	strcat (buff, "Cancel##-1");
		
	int item = grlib_menu ("Sort and display options...", buff);
	
	if (item == 4)
		{
		showHidden = 1 - showHidden;
		AppsBrowse ();
		}
		
	if (item == 2)
		{
		int item = grlib_menu ("Sort mode:\n\n"
								"Plese click (A) on every application in the order\n"
								"you want them to be displayed.\n"
								"Click (B) to exit sort mode", 
								"OK|Cancel");
		if (item == 0) 
			sortMode = 1000; // Umph... maybe someone has more than 1000 apps ? 
		}
	if (item == 3)
		{
		int item = grlib_menu ("Sort application by name:\n\n"
								"If you press OK, applications will\n"
								"be sorted by name",
								"OK|Cancel");
		if (item  == 0)
			{
			int i;
			for (i = 0; i < appsCnt; i++)
				{
				apps[i].needUpdate = TRUE;
				apps[i].priority = 1;
				}
			
			AppsSort();
			}
		}
	}

static void ShowMainMenu (void)
	{
	char buff[300];
	
	buff[0] = '\0';
	
	grlib_menuAddItem (buff, 100, "Switch to Channel mode");
	grlib_menuAddItem (buff, 101, "Switch to Game mode");
	grlib_menuAddSeparator (buff);
	grlib_menuAddItem (buff,  0, "Rescan devices");
	grlib_menuAddSeparator (buff);
	grlib_menuAddItem (buff,  4, "Run WII system menu");
	grlib_menuAddItem (buff, 20, "Run BOOTMII");
	grlib_menuAddItem (buff, 21, "Run DISC");
	grlib_menuAddSeparator (buff);
	grlib_menuAddItem (buff,  6, "Sort and display options...");
	grlib_menuAddItem (buff,  5, "Options...");
	grlib_menuAddSeparator (buff);
	grlib_menuAddItem (buff, -1, "Close");
		
	int item = grlib_menu ("Homebrew menu'", buff);
		
	if (item == 0)
		{
		MasterInterface (1, 0, 2, "Please wait...");
		MountDevices(0);
		AppsBrowse ();
		}
		
	if (item == 100)
		{
		browserRet = INTERACTIVE_RET_TOCHANNELS;
		}
		
	if (item == 101)
		{
		browserRet = INTERACTIVE_RET_TOGAMES;
		}
		
	if (item == 20)
		{
		browserRet = INTERACTIVE_RET_BOOTMII;
		}

	if (item == 21)
		{
		browserRet = INTERACTIVE_RET_DISC;
		}

	if (item == 4)
		{
		browserRet = INTERACTIVE_RET_HOME;
		}
		
	if (item == 5)
		{
		ShowAboutMenu ();
		}

	if (item == 6)
		{
		SortDispMenu ();
		}

	}

static int FindSpot (void)
	{
	int i;
	static time_t t = 0;
	
	appsSelected = -1;
	spotSelected = -1;
	
	//grlib_printf (XMIDLEINFO, theme.hb_line3Y, GRLIB_ALIGNCENTER, 0, "[[ %d ]]",gui.spotsIdx);
	
	for (i = 0; i < gui.spotsIdx; i++)
		{
		if (grlib_irPos.x > gui.spots[i].ico.rx1 && grlib_irPos.x < gui.spots[i].ico.rx2 && grlib_irPos.y > gui.spots[i].ico.ry1 && grlib_irPos.y < gui.spots[i].ico.ry2)
			{
			// Ok, we have the point
			appsSelected = gui.spots[i].id;
			spotSelected = i;
			/*
			gui.spots[i].ico.sel = true;
			grlib_IconDraw (&is, &gui.spots[i].ico);
			*/
			
			grlib_SetFontBMF (fonts[FNTNORM]);
			
			if (apps[appsSelected].desc)
				{
				grlib_printf (XMIDLEINFO, theme.hb_line1Y, GRLIB_ALIGNCENTER, 0, apps[appsSelected].name);
				grlib_printf (XMIDLEINFO, theme.hb_line2Y, GRLIB_ALIGNCENTER, 0, apps[appsSelected].desc);
				}
			else
				grlib_printf (XMIDLEINFO, theme.hb_line1Y, GRLIB_ALIGNCENTER, 0, apps[appsSelected].name);
				
			grlib_SetFontBMF (fonts[FNTSMALL]);
			
			if (apps[appsSelected].type != AT_FOLDERUP)
				{
				if (theme.hb_line3Y > 0)
					{
					char rld[32];
					
					if (apps[appsSelected].iosReload)
						strcpy (rld, "");
					else
						strcpy (rld, " <no_ios_reload/>");
					
					if (apps[appsSelected].args != NULL)
						grlib_printf (XMIDLEINFO, theme.hb_line3Y, GRLIB_ALIGNCENTER, 0, "/apps/%s%s (%s)%s",subpath, apps[appsSelected].path, apps[appsSelected].args, rld);
					else
						grlib_printf (XMIDLEINFO, theme.hb_line3Y, GRLIB_ALIGNCENTER, 0, "/apps/%s%s%s",subpath, apps[appsSelected].path, rld);
					}
				}
			
			t = time(NULL);
			break;
			}
		}
	
	grlib_SetFontBMF (fonts[FNTNORM]);
	if (!grlib_irPos.valid)
		{
		if (appsSelected == -1) grlib_printf (XMIDLEINFO, theme.hb_line2Y, GRLIB_ALIGNCENTER, 0, "Point an icon with the wiimote or use a GC controller!");		
		}
	else
		if (time(NULL) - t > 0 && appsSelected == -1)
			{
			grlib_printf (XMIDLEINFO, theme.hb_line2Y, GRLIB_ALIGNCENTER, 0, "(A) Run application (B) App. menu (1) to channel mode");
			}
	
	if (spotSelectedLast != spotSelected)
		redraw = 1;
		
	spotSelectedLast = spotSelected;
	
	return appsSelected;
	}

static void Redraw (void)
	{
	int i;
	int ai;	// Application index (corrected by the offset)
	char sdev[64];

	Debug ("Redraw [begin]");

	if (!theme.ok)
		Video_DrawBackgroud (1);
	else
		GRRLIB_DrawImg( 0, 0, theme.bkg, 0, 1, 1, RGBA(255, 255, 255, 255) ); 

	if (!subpath[0])
		{
		strcpy (sdev, "");
		for (i = 0; i < DEVMAX; i++)
			if (IsDevValid(i)) 
				{
				strcat (sdev, "[");
				strcat (sdev, vars.mount[i]);
				strcat (sdev, "] ");
				}
		}
	else
		{
		sprintf (sdev, "[%s] %s", submount, subpath);
		}
		
	grlib_SetFontBMF(fonts[FNTNORM]);
	
	char ahpbrot[16];
	if (vars.ahbprot)
		strcpy (ahpbrot," AHPBROT");
	else
		strcpy (ahpbrot,"");
		
	if (vars.neek == NEEK_NONE)
		grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader"VER" (IOS%d%s) - %s", vars.ios, ahpbrot, sdev);
	else
		{
		char neek[32];
		
		if (vars.neek == NEEK_2o)
			strcpy (neek, "neek2o");
		else
			strcpy (neek, "neek");
			
		if (strlen(vars.neekName))
			grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader"VER" (%s %s) - %s", neek, vars.neekName, sdev);
		else
			grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader"VER" (%s) - %s", neek, sdev);
		}
		
	grlib_printf ( 615, 26, GRLIB_ALIGNRIGHT, 0, "Page %d of %d", appsPage+1, appsPageMax+1);
	
	Debug ("Redraw [spots]");

	// Prepare black box
	s_grlib_icon ico;
	for (i = 0; i < gui.spotsXpage; i++)
		if (spotSelected != i)
			{
			// Make sure that icon is not in sel state, and clean any ambiguity
			gui.spots[i].ico.sel = false;
			gui.spots[i].ico.title[0] = '\0';
			gui.spots[i].ico.iconOverlay[0] = NULL;
			gui.spots[i].ico.iconOverlay[1] = NULL;
			gui.spots[i].ico.iconOverlay[2] = NULL;
			
			grlib_IconInit (&ico, &gui.spots[i].ico);

			ico.noIcon = true;
			grlib_IconDraw (&is, &ico);
			}
	
	gui.spotsIdx = 0;

	//Debug ("Redraw [icons]");

	for (i = 0; i < gui.spotsXpage; i++)
		{
		ai = (appsPage * gui.spotsXpage) + i;
		
		if (ai < appsCnt && ai < apps2Disp && gui.spotsIdx < SPOTSMAX)
			{
			if (gui.spots[gui.spotsIdx].id != ai)
				{
				if (gui.spots[gui.spotsIdx].ico.icon) GRRLIB_FreeTexture (gui.spots[gui.spotsIdx].ico.icon);
				gui.spots[gui.spotsIdx].ico.icon = NULL;
				
				if (apps[ai].type != AT_FOLDERUP)
					{
					gui.spots[gui.spotsIdx].ico.icon = GetTexture (ai);
					gui.spots[gui.spotsIdx].ico.alticon = NULL;
					}

				if (apps[ai].type == AT_FOLDERUP) 
					gui.spots[gui.spotsIdx].ico.alticon = vars.tex[TEX_FOLDERUP];

				if (apps[ai].type == AT_FOLDER) 
					gui.spots[gui.spotsIdx].ico.alticon = vars.tex[TEX_FOLDER];
				}

			//Debug ("Drawing #1");

			// Check if it is autoboot app
			if (config.autoboot.enabled && config.autoboot.appMode == APPMODE_HBA)
				{
				char path[300];
				sprintf (path,"%s:/apps/%s%s/", apps[ai].mount, subpath, apps[ai].path);
				if (strcmp (config.autoboot.path, path) == 0)
					gui.spots[gui.spotsIdx].ico.iconOverlay[0] = vars.tex[TEX_STAR];
				}

			//Debug ("Drawing #2");

			// Is it hidden ?
			if (apps[ai].hidden && showHidden)
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = vars.tex[TEX_GHOST];
				
			// device icon
			if (apps[ai].mount[0] == 's' || apps[ai].mount[0] == 'S') 
				gui.spots[gui.spotsIdx].ico.iconOverlay[2] = vars.tex[TEX_SD];

			if (apps[ai].mount[0] == 'u' || apps[ai].mount[0] == 'U') 
				gui.spots[gui.spotsIdx].ico.iconOverlay[2] = vars.tex[TEX_USB];
			
			//Debug ("Drawing #3");

			if (!gui.spots[gui.spotsIdx].ico.icon)
				{
				if (apps[ai].name)
					strcpy (gui.spots[gui.spotsIdx].ico.title, apps[ai].name);
				else
					strcpy (gui.spots[gui.spotsIdx].ico.title, apps[ai].path);
				}

			//Debug ("Drawing #4");
			if (spotSelected == i)
				gui.spots[i].ico.sel = true;
				
			grlib_IconDraw (&is, &gui.spots[gui.spotsIdx].ico);
			
			gui.spots[gui.spotsIdx].id = ai;
			gui.spotsIdx++;
			}
		}

	grlib_SetFontBMF(fonts[FNTNORM]);

	//Debug ("Redraw [end]");
	}

static void Overlay (void)
	{
	Video_DrawWIFI ();
	
	return;
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
	
	//memset (&apps, 0, sizeof (apps));
	apps = calloc (APPSMAX, sizeof(s_app));
	
	subpath[0] = '\0';
	submount[0] = '\0';
	config.run.enabled = 0;
	
	// Immediately draw the screen...
	AppsFree ();
	InitializeGui ();
	
	Redraw ();
	grlib_PushScreen ();
	grlib_PopScreen ();
	grlib_Render();  // Render the frame buffer to the TV
	
	AppsBrowse ();
	
	ConfigWrite ();
	
	if (config.appPage >= 0 && config.appPage <= appsPageMax)
		appsPage = config.appPage;
	else
		appsPage = 0;

   // Loop forever
    while (browserRet == -1) 
		{
		btn = grlib_GetUserInput();
		
		// If [HOME] was pressed on the first Wiimote, break out of the loop
		if (btn)
			{
			while (WPAD_ButtonsDown(0)) WPAD_ScanPads();
			
			if (btn & WPAD_BUTTON_1) 
				{
				browserRet = INTERACTIVE_RET_TOCHANNELS;
				}

			if (btn & WPAD_BUTTON_A && appsSelected != -1 && sortMode == 0) 
				{
				if (apps[appsSelected].type == AT_HBA)
					{
					AppsSetRun (appsSelected);
					browserRet = INTERACTIVE_RET_HBSEL;
					break;
					}
				else if (apps[appsSelected].type == AT_FOLDER) // This is a folder ! Jump inside
					{	
					sprintf (subpath, "%s/", apps[appsSelected].path);
					sprintf (submount, "%s", apps[appsSelected].mount);
					
					AppsBrowse ();
					redraw = 1;
					}
				else if (apps[appsSelected].type == AT_FOLDERUP) // This is a folder ! Jump inside
					{	
					int i = strlen(subpath);
					
					if (i > 0) i--;
					if (i > 0) i--;
					
					while (i >= 0 && subpath[i] != '/')
						subpath[i--] = 0;
					
					gui.spotsIdx = 0;
					AppsBrowse ();
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
				AppsSort();
				
				int i;
				for (i = 0; i < appsCnt; i++)
					{
					apps[i].priority = appsCnt - i;
					apps[i].needUpdate = TRUE;
					apps[i].checked = FALSE;
					}
				sortMode = 0;
				
				SaveSettings ();
				
				redraw = 1;
				}

			if (btn & WPAD_BUTTON_HOME && sortMode > 0)
				{
				grlib_menu ("You are in sort mode.\n", "Close");
				redraw = 1;
				}

			if (btn & WPAD_BUTTON_HOME && sortMode == 0)
				{
				ShowMainMenu ();
				redraw = 1;
				}
			
			if (btn & WPAD_BUTTON_PLUS) {appsPage++; redraw = 1;}
			if (btn & WPAD_BUTTON_MINUS)  {appsPage--; redraw = 1;}

			}
		
		FindSpot ();

		if (redraw)
			{
			if (appsPage < 0)
				{
				appsPage = 0;
				continue;
				}
			if (appsPage > appsPageMax)
				{
				appsPage = appsPageMax;
				continue;
				}
			
			Redraw ();
			grlib_PushScreen ();
			redraw = 0;
			}
		
		grlib_PopScreen ();
		Overlay ();
		grlib_DrawIRCursor ();
		//grlib_printf (10,450,GRLIB_ALIGNLEFT, 0, "ticks: %u", get_msec(false));

        grlib_Render();  // Render the frame buffer to the TV
		
		if (grlibSettings.wiiswitch_poweroff || grlibSettings.wiiswitch_reset)
			{
			browserRet = INTERACTIVE_RET_SHUTDOWN;
			break;
			}
			
		if (wiiload.status == WIILOAD_HBZREADY)
			{
			WiiloadZipMenu ();
			AppsBrowse ();
			redraw = 1;
			}
			
		if (wiiload.status == WIILOAD_HBREADY && WiiloadPostloaderDolMenu())
			{
			browserRet = INTERACTIVE_RET_WIILOAD;
			redraw = 1;
			break;
			}
		
		usleep (5000);
		}

	// save current page
	config.appPage = appsPage;

	SaveSettings ();
	
	// Clean up all data
	AppsFree ();
	gui_Clean ();
	free (apps);
	
	grlib_SetRedrawCallback (NULL, NULL);
	return browserRet;
	}