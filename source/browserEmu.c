#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <dirent.h>
#include <ogc/lwp_watchdog.h>

#include "wiiload/wiiload.h"
#include "globals.h"
#include "bin2o.h"
#include "network.h"
#include "sys/errno.h"
#include "http.h"
#include "ios.h"
#include "identify.h"
#include "gui.h"
#include "neek.h"
#include "mystring.h"
#include "cfg.h"
#include "devices.h"
#include "browser.h"
#include "fsop/fsop.h"

#define EMUMAX 16384

/*

This allow to browse for applications in emus folder of mounted drive

*/
extern s_grlibSettings grlibSettings;
static s_emuConfig emuConf;

static int browse = 0;
static int scanned = 0;

#define CATMAX 32

static s_emu *emus;
static int emusCnt;
static int emus2Disp;
static int page; // Page to be displayed
static int pageMax; // 
static int emuSelected = -1;	// Current selected app with wimote

static int browserRet = 0;
static int showHidden = 0;

static u8 redraw = 1;
static bool redrawIcons = true;

static s_grlib_iconSetting is;
\
static void Redraw (void);
static int EmuBrowse (void);
static bool IsFiltered (int ai);

static int disableSpots;

static s_cfg *cfg;

static s_cfg *plugins;
static int pluginsCnt = 0;

static GRRLIB_texImg **emuicons;

static int usedBytes = 0;

#define ICONW 100
#define ICONH 93

#define XMIDLEINFO 320
#define INIT_X (30 + ICONW / 2)
#define INIT_Y (60 + ICONH / 2)
#define INC_X ICONW+22
#define INC_Y ICONH+25

static void InitializeGui (void)
	{
	// Prepare black box
	int i, il;
	int x = INIT_X;
	int y = INIT_Y;

	gui_Init();

	gui.spotsIdx = 0;
	gui.spotsXpage = 15;
	gui.spotsXline = 5;

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
	is.fontReverse = 0; 		// Border color

	il = 0;
	for (i = 0; i < gui.spotsXpage; i++)
		{
		grlib_IconInit (&gui.spots[i].ico, NULL);
		
		gui.spots[i].ico.x = x;
		gui.spots[i].ico.y = y;
		gui.spots[i].ico.w = ICONW;
		gui.spots[i].ico.h = ICONH;
		gui.spots[i].ico.titleAlign = GRLIB_ALIGNICOTEXT_BOTTOM;

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

static void Conf (bool open)
	{
	char cfgpath[64];
	sprintf (cfgpath, "%s://ploader/emus.conf", vars.defMount);

	cfg_Section (NULL);
	if (open)
		{
		cfg = cfg_Alloc (cfgpath, 16384, 0, 0);
		}
	else
		{
		cfg_Store (cfg, cfgpath);
		cfg_Free (cfg);
		}
	}
	
static void WriteGameConfig (int ia)
	{
	if (ia < 0) return;
	
	strcpy (emuConf.name, emus[ia].name);
	emuConf.hidden = emus[ia].hidden;
	emuConf.priority = emus[ia].priority;
	//emuConf.category = emus[ia].category;
	emuConf.playcount = emus[ia].playcount;
	
	char *buff = Bin2HexAscii (&emuConf, sizeof (s_emuConfig), 0);
	cfg_SetString (cfg, fsop_GetFilename(emus[ia].name, true), buff);
	free (buff);
	}

static int ReadGameConfig (int ia)
	{
	char buff[1024];
	bool valid;
	
	valid = cfg_GetString (cfg, fsop_GetFilename(emus[ia].name, true), buff);
	
	if (valid)
		{
		if (HexAscii2Bin (buff, &emuConf) > sizeof (s_emuConfig))
			{
			valid = false;
			}
		}
	
	if (!valid)
		{
		emuConf.priority = 5;
		emuConf.hidden = 0;
		emuConf.playcount = 0;
		//emuConf.category = 0;
		}

	//emus[ia].category = emuConf.category;
	emus[ia].hidden = emuConf.hidden;
	emus[ia].priority = emuConf.priority;
	emus[ia].playcount = emuConf.playcount;
	
	return valid;
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PIN_NAME 0
#define PIN_DOL 1
#define PIN_PATH 2
#define PIN_EXT 3
#define PIN_ICON 4

static char *Plugins_Get (int idx, int type)
	{
	static char name[128];
	char *p;
	
	if (!plugins->items[idx])
		{
		return NULL;
		}
		
	*name = '\0';
	p = ms_GetDelimitedString (plugins->items[idx], ';', type);
	if (p)
		{
		strcpy (name, p);
		free (p);
		}

	return name;
	}
	
static int Plugins_GetId (int idx)
	{
	if (!plugins->items[idx])
		{
		return 0;
		}
	
	return atoi(plugins->tags[idx]);
	}
	
static void Plugins (bool open)
	{
	char cfgpath[64];
	char path[256];
	int i;
	
	sprintf (cfgpath, "%s://ploader/plugins.conf", vars.defMount);

	if (open)
		{
		plugins = cfg_Alloc (cfgpath, 64, 0, 1);
		
		pluginsCnt = plugins->count;
		// just for debug
		for (i = 0; i < plugins->count; i++)
			Debug ("Plugins: %d:%d -> %s", i, pluginsCnt, plugins->items[i]);
			
		if (pluginsCnt > CATMAX)
			pluginsCnt = CATMAX;
			
		emuicons = calloc (sizeof (GRRLIB_texImg), pluginsCnt);
		for (i = 0; i < pluginsCnt; i++)
			{
			sprintf (path, "%s://ploader/theme/%s", vars.defMount, Plugins_Get(i, PIN_ICON));
			emuicons[i] = GRRLIB_LoadTextureFromFile (path);

			if (!emuicons[i])
				{
				sprintf (path, "%s://ploader/plugins/%s", vars.defMount, Plugins_Get(i, PIN_ICON));
				emuicons[i] = GRRLIB_LoadTextureFromFile (path);
				}
			}
		
		}
	else
		{
		int i;
		
		for (i = 0; i < pluginsCnt; i++)
			GRRLIB_FreeTexture (emuicons[i]);
		
		free (emuicons);
			
		cfg_Free (plugins);
		}
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int CountRomsPlugin (int type)
	{
	int i;
	int count = 0;
	
	for (i = 0; i < emusCnt; i++)
		{
		if (emus[i].type == type) count++;
		}
	
	return count;
	}
	
static void MakeCoverPath (int ai, char *path)
	{
	sprintf (path, "%s://ploader/covers.emu/%s.png", vars.defMount, fsop_GetFilename(emus[ai].name, true));
	}

static void FeedCoverCache (void)
	{
	char path[256];
	CoverCache_Pause (true);

	if (page > pageMax) page = pageMax;
	if (page < 0) page = 0;

	int i;
	int ai;	// Application index (corrected by the offset)

	for (i = -gui.spotsXpage; i < gui.spotsXpage*2; i++)
		{
		ai = (page * gui.spotsXpage) + i;
		
		if (ai >= 0 && ai < emusCnt && emus[ai].hasCover)
			{
			MakeCoverPath (ai, path);
			CoverCache_Add (path, (i >= 0 && i < gui.spotsXpage) ? true:false );
			}
		}
	
	CoverCache_Pause (false);
	}

static void StructFree (void)
	{
	int i;
	
	for (i = 0; i < emusCnt; i++)
		{
		emus[i].png = NULL;
		
		if (emus[i].name != NULL) 
			{
			free (emus[i].name);
			emus[i].name = NULL;
			}
		}
		
	emusCnt = 0;
	}
	
////////////////////////////////////////////////////////////////////////////////

static void GetCovers_Scan (char *path)
	{
	DIR *pdir;
	struct dirent *pent;
	char fn[256];
	char romname[256];
	char cover[256];
	int i;
	time_t t = 0;
	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0 || ms_strstr (pent->d_name, ".png") == NULL)
			continue;
			
		for (i = 0; i < emusCnt; i++)
			{
			if (time(NULL) > t)
				{
				strcpy (romname, fsop_GetFilename (emus[i].name, true));
				romname[32] = 0;
				Video_WaitPanel (TEX_HGL, "Searching...|(B) Stop", romname, emusCnt);
				t = time(NULL)+1;
				}

			strcpy (romname, fsop_GetFilename (emus[i].name, true));
			
			if (ms_strstr (pent->d_name, romname))
				{
				sprintf (fn, "%s/%s", path, pent->d_name);
				
				size_t size;
				fsop_GetFileSizeBytes (fn, &size);
				
				if (size > 128)
					{
					MakeCoverPath (i, cover);
					fsop_CopyFile (fn, cover, 0);
					unlink (fn);
					Debug ("GetCovers %s -> %s", fn, cover);
					}
				else
					{
					Debug ("GetCovers %s is to small", fn);
					}
				}
			}
		}
	closedir(pdir);
	
	FeedCoverCache ();
	redraw = 1;
	}
	
static void GetCovers (void)
	{
	char pngpath[256];
	int dev;
	
	for (dev = 0; dev < DEV_MAX; dev++)
		{
		if (devices_Get (dev))
			{
			sprintf (pngpath, "%s://snes9xgx/saves", devices_Get (dev));
			GetCovers_Scan (pngpath);

			sprintf (pngpath, "%s://fceugx/saves", devices_Get (dev));
			GetCovers_Scan (pngpath);

			sprintf (pngpath, "%s://vbagx/saves", devices_Get (dev));
			GetCovers_Scan (pngpath);

			sprintf (pngpath, "%s://genplus/snaps/gg", devices_Get (dev));
			GetCovers_Scan (pngpath);
			sprintf (pngpath, "%s://genplus/snaps/md", devices_Get (dev));
			GetCovers_Scan (pngpath);
			sprintf (pngpath, "%s://genplus/snaps/ms", devices_Get (dev));
			GetCovers_Scan (pngpath);
			sprintf (pngpath, "%s://genplus/snaps/sg", devices_Get (dev));
			GetCovers_Scan (pngpath);

			sprintf (pngpath, "%s://wii64/saves", devices_Get (dev));
			GetCovers_Scan (pngpath);
			}
		}
	
	CoverCache_Flush ();
	EmuBrowse ();
	redraw = 1;
	}

////////////////////////////////////////////////////////////////////////////////

static int qsort_name (const void * a, const void * b)
	{
	s_emu *aa = (s_emu*) a;
	s_emu *bb = (s_emu*) b;
	return (ms_strcmp (strrchr (aa->name, '/') + 1, strrchr (bb->name, '/') + 1));
	}
	
static int bsort_filter (const void * a, const void * b)
	{
	s_emu *aa = (s_emu*) a;
	s_emu *bb = (s_emu*) b;

	if (aa->filtered < bb->filtered) return 1;
	
	return 0;
	}
	
static int bsort_hidden (const void * a, const void * b)
	{
	s_emu *aa = (s_emu*) a;
	s_emu *bb = (s_emu*) b;

	if (aa->filtered > bb->filtered) return 1;
	
	return 0;
	}
	
static void SortItems (void)
	{
	int i;
	int filtered = 0;
	
	// Apply filters
	emus2Disp = 0;
	for (i = 0; i < emusCnt; i++)
		{
		emus[i].filtered = IsFiltered (i);
		if (emus[i].filtered && (!emus[i].hidden || showHidden)) emus2Disp++;
		if (emus[i].filtered) filtered++;
		}
	
	Video_WaitIcon (TEX_HGL);
	bsort (emus, emusCnt, sizeof(s_emu), bsort_filter);

	Video_WaitIcon (TEX_HGL);
	bsort (emus, filtered, sizeof(s_emu), bsort_hidden);

	Video_WaitIcon (TEX_HGL);
	qsort (emus, emus2Disp, sizeof(s_emu), qsort_name);

	pageMax = (emus2Disp-1) / gui.spotsXpage;
	
	FeedCoverCache ();
	}
	
// This will check if cover is available
static void CheckForCovers (void)
	{
	DIR *pdir;
	struct dirent *pent;

	char path[256];
	int i;
	
	// Cleanup cover flag
	for (i = 0; i < emusCnt; i++)
		emus[i].hasCover = 0;
		
	sprintf (path, "%s://ploader/covers.emu", vars.defMount);
	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0 || ms_strstr (pent->d_name, ".png") == NULL)
			continue;

		for (i = 0; i < emusCnt; i++)
			{
			if (!emus[i].hasCover && ms_strstr (pent->d_name, fsop_GetFilename (emus[i].name, true)))
				{
				emus[i].hasCover = 1;
				break;
				}
			}
		}
	closedir(pdir);
	}
	
static int BrowsePluginFolder (int type, int startidx, char *path)
	{
	int i = startidx;
	DIR *pdir;
	struct dirent *pent;
	char fn[300];
	int updater = 0;
	char ext[256];
	char *p;
	
	strcpy (ext, Plugins_Get(type, PIN_EXT));

	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if (i >= EMUMAX)
			break;
		
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
			
		p = pent->d_name + strlen(pent->d_name) - 1;

		while (p > pent->d_name && *p != '.') p--;
		
		if (*p != '.')
			continue;
			
		p++;
		
		if (!ms_strstr (ext, p))
			continue;

		sprintf (fn, "%s/%s", path, pent->d_name);
		
		//Debug ("   > %s", fn);
		
		emus[i].name = calloc (1, strlen (fn) + 1);
		
		usedBytes += (strlen (fn) + 1);
		
		if (!emus[i].name)
			{
			Debug ("allocation error (%s)", pent->d_name);
			continue;
			}
		strcpy (emus[i].name, fn);
		
		emus[i].type = type;
		
		if (++updater > 100)
			{
			Video_WaitIcon (TEX_HGL);
			updater = 0;
			}
		
		i++;
		}
		
	closedir (pdir);
		
	return i-startidx;
	}
	
static int EmuBrowse (void)
	{
	int i, cnt;
	Debug ("begin EmuBrowse");
	
	gui.spotsIdx = 0;
	gui_Clean ();
	StructFree ();

	Video_WaitIcon (TEX_HGL);

	char path[300];
	int dev;
	
	Debug ("Emu Browse: searching for plugins data roms");
	
	usedBytes = 0;
	emusCnt = 0;
	for (dev = 0; dev < DEV_MAX; dev++)
		{
		if (devices_Get (dev))
			{
			Debug ("Checking: %s (%d)", devices_Get (dev), pluginsCnt);
			
			for (i = 0; i < pluginsCnt; i++)
				{
				sprintf (path, "%s:/%s", devices_Get (dev), Plugins_Get (i, PIN_PATH));
				cnt = BrowsePluginFolder (Plugins_GetId(i), emusCnt, path);
				emusCnt += cnt;
				Debug ("found %d roms in %s", cnt, path);
				}
			}
		}
			
	scanned = 1;
	
	Debug ("Allocated %d bytes (%d Kb)", EMUMAX * sizeof(s_emu) + usedBytes, (EMUMAX * sizeof(s_emu) + usedBytes) / 1024);
	
	CheckForCovers ();
	SortItems ();

	Debug ("end EmuBrowse");

	return emusCnt;
	}

static int FindSpot (void)
	{
	int i;
	char buff[300];
	static time_t t = 0;

	emuSelected = -1;
	
	if (disableSpots) return emuSelected;
	
	for (i = 0; i < gui.spotsIdx; i++)
		{
		if (grlib_irPos.x > gui.spots[i].ico.rx1 && grlib_irPos.x < gui.spots[i].ico.rx2 && grlib_irPos.y > gui.spots[i].ico.ry1 && grlib_irPos.y < gui.spots[i].ico.ry2)
			{
			// Ok, we have the point
			
			emuSelected = gui.spots[i].id;

			Video_SetFont(TTFVERYSMALL);
			gui.spots[i].ico.sel = true;
			grlib_IconDraw (&is, &gui.spots[i].ico);

			Video_SetFont(TTFNORM);
			sprintf (buff, "%s: %s", Plugins_Get (emus[emuSelected].type, PIN_NAME), fsop_GetFilename (emus[emuSelected].name, true));

			char title[256];
			strcpy (title, buff);
			if (strlen (title) > 48)
				{
				title[48] = 0;
				strcat (title, "...");
				}
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, title);

			Video_SetFont(TTFSMALL);
			grlib_printf (XMIDLEINFO, theme.line1Y, GRLIB_ALIGNCENTER, 0, fsop_GetPath(emus[emuSelected].name));
			
			t = time(NULL);
			break;
			}
		}
	
	Video_SetFont(TTFSMALL);
	if (!grlib_irPos.valid)
		{
		if (emuSelected == -1) grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "Point an icon with the wiimote or use a GC controller!");
		}
	else
		{
		if (time(NULL) - t > 0 && emuSelected == -1)
			{
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "(A) Execute (B) Settings (D-Pad) Switch mode (1) goto page (2) Filters");
			}
		}
	return emuSelected;
	}
	
static bool IsFiltered (int ai)
	{
	int i;
	bool ret = false;

	if (config.emuFilter == 0) return true;
	
	for (i = 0; i < CATMAX; i++)
		{
		if ((config.emuFilter & (1 << i)) && (emus[ai].type == i))
			{
			ret = true;
			}
		}

	return ret;
	}
	
static void ShowFilterMenu (void)
	{
	char buff[2048];
	u8 f[CATMAX];
	int i, item;
	
	for (i = 0; i < pluginsCnt; i++)
		f[i] = 0;
	
	for (i = 0; i < pluginsCnt; i++)
		f[i] = (config.emuFilter & (1 << i)) ? 1:0;

	do
		{
		buff[0] = '\0';
		int col = 0;
		for (i = 0; i < pluginsCnt; i++)
			{
			if (col == 8 || col == 16 || col == 24) grlib_menuAddColumn (buff);
			//grlib_menuAddCheckItem (buff, 100 + i, f[i], "%s: %s", Plugins_GetName (i), Plugins_GetPath (i));
			if (CountRomsPlugin(i))
				{
				grlib_menuAddCheckItem (buff, 100 + i, f[i], "%s", Plugins_Get (i, PIN_NAME));
				col ++;
				}
			}
		
		Video_SetFontMenu(TTFVERYSMALL);
		item = grlib_menu (0, "Filter menu\nPress (B) to close, (+) Select all, (-) Deselect all (shown all emus)", buff);
		Video_SetFontMenu(TTFNORM);

		if (item == MNUBTN_PLUS)
			{
			int i; 	for (i = 0; i < pluginsCnt; i++) f[i] = 1;
			}

		if (item == MNUBTN_MINUS)
			{
			int i; 	for (i = 0; i < pluginsCnt; i++) f[i] = 0;
			}
		
		if (item >= 100)
			{
			int i = item - 100;
			f[i] = !f[i];
			}
		}
	while (item != -1);
	
	config.emuFilter = 0;
	for (i = 0; i < pluginsCnt; i++)
		if (f[i]) config.emuFilter |= (1 << i);
	
	EmuBrowse ();
	}

static void ShowAppMenu (int ai)
	{
	char buff[512];
	char title[256];
	
	strcpy (title, emus[ai].name);
	if (strlen(title) > 64)
		{
		title[64] = 0;
		strcat (title, "...");
		}
	
	ReadGameConfig (ai);
	do
		{
		buff[0] = '\0';

		grlib_menuAddItem (buff,  1, "Played %d times", emus[ai].playcount);
		
		if (CheckParental(0))
			{
			grlib_menuAddSeparator (buff);
			grlib_menuAddItem (buff,  2, "Delete this rom (and cover)");
			grlib_menuAddItem (buff,  3, "Delete this cover");
			}
		
		int item = grlib_menu (0, title, buff);
		
		if (item < 0) break;
		
		if (item == 1)
			{
			int item = grlib_menu (-60, "Reset play count ?", "Yes~No");
			
			if (item == 0)
				emus[ai].playcount = 0;
			}
			
		if (item == 2)
			{
			sprintf (buff, "Are you sure you want to delete this ROM and it's cover ?\n\n%s", title);
			int item = grlib_menu (-60, buff, "Yes~No");
			
			if (item == 0)
				{
				unlink (emus[ai].name);
				
				char cover[256];
				MakeCoverPath (ai, cover);
				
				unlink (cover);
				
				EmuBrowse();
				break;
				}
			}
		
		if (item == 3)
			{
			sprintf (buff, "Are you sure you want to delete this ROM's cover ?\n\n%s", title);
			int item = grlib_menu (-60, buff, "Yes~No");
			
			if (item == 0)
				{
				char cover[256];
				MakeCoverPath (ai, cover);
				
				unlink (cover);

				EmuBrowse();
				break;
				}
			}
		}
	while (TRUE);
	
	WriteGameConfig (ai);
	SortItems ();

	return;
	}

static void ShowMainMenu (void)
	{
	char buff[512];
	
	start:
	
	buff[0] = '\0';
	
	if (config.gameSort == 0)
		grlib_menuAddItem (buff,  9, "Sort by: vote");
	if (config.gameSort == 1)
		grlib_menuAddItem (buff,  9, "Sort by: name");
	if (config.gameSort == 2)
		grlib_menuAddItem (buff,  9, "Sort by: play count");
	
	grlib_menuAddItem (buff,  3, "Select filters");

	if (CheckParental(0))
		{
		if (showHidden)
			grlib_menuAddItem (buff,  6, "Hide hidden emus");
		else
			grlib_menuAddItem (buff,  7, "Show hidden emus");
			
		grlib_menuAddItem (buff,  1, "Import snapshots as covers");
		}

	Redraw();
	grlib_PushScreen();
	
	int item = grlib_menu (0, "Emulators menu", buff);
	
	if (item == 1)
		{
		GetCovers ();
		goto start;
		}

	if (item == 9)
		{
		config.gameSort ++;
		if (config.gameSort > 2) config.gameSort = 0;
		goto start;
		}

	if (item == 3)
		{
		ShowFilterMenu ();
		goto start;
		}
		
	if (item == 6)
		{
		showHidden = 0;
		}

	if (item == 7)
		{
		showHidden = 1;
		}
		
	SortItems ();
	redraw = 1;
	}

static void RedrawIcons (int xoff, int yoff)
	{
	int i;
	int ai;	// Application index (corrected by the offset)
	char path[256];
	
	Video_SetFont(TTFVERYSMALL);

	// Prepare black box
	for (i = 0; i < gui.spotsXpage; i++)
		{
		// Make sure that icon is not in sel state
		gui.spots[i].ico.sel = false;
		gui.spots[i].ico.title[0] = '\0';
		gui.spots[i].ico.xoff = xoff;
		}
	
	// Draw Icons
	gui.spotsIdx = 0;
	for (i = 0; i < gui.spotsXpage; i++)
		{
		ai = (page * gui.spotsXpage) + i;
		
		if (ai < emusCnt && ai < emus2Disp && gui.spotsIdx < SPOTSMAX)
			{
			MakeCoverPath (ai, path);
			gui.spots[gui.spotsIdx].ico.icon = CoverCache_Get(path);
				
			if (!gui.spots[gui.spotsIdx].ico.icon)
				{
				gui.spots[gui.spotsIdx].ico.icon = emuicons[emus[ai].type];

				char title[256];
				strcpy (title, fsop_GetFilename(emus[ai].name, true));
				title[16] = 0;
				strcpy (gui.spots[gui.spotsIdx].ico.title, title);
				}
			else
				gui.spots[gui.spotsIdx].ico.title[0] = '\0';

			// Is it hidden ?
			if (emus[ai].hidden && showHidden)
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = vars.tex[TEX_GHOST];
			else
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = NULL;
				
			grlib_IconDraw (&is, &gui.spots[gui.spotsIdx].ico);
			
			// Let's add the spot points, to reconize the icon pointed by wiimote
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
	/*
	if (emusCnt == 0)
		{
		grlib_DrawCenteredWindow ("No emus found: Check /ploader/plugins.conf", WAITPANWIDTH, 133, 0, NULL);
		Video_DrawIcon (TEX_EXCL, 320, 250);
		}
	*/
	}

static void Redraw (void)
	{
	Video_DrawBackgroud (1);
	
	Video_SetFont(TTFNORM);

	grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader::Emulators");
	grlib_printf ( 615, 26, GRLIB_ALIGNRIGHT, 0, "Page %d of %d", page+1, pageMax+1);
	
	if (redrawIcons) RedrawIcons (0,0);
	
	Video_DrawVersionInfo ();
	
	if (emusCnt == 0 && scanned)
		{
		Video_SetFont(TTFNORM);
		grlib_DrawCenteredWindow ("No emus found, check /ploader/plugins.conf", WAITPANWIDTH+50, 133, 0, NULL);
		Video_DrawIcon (TEX_EXCL, 320, 250);
		}
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
	
	GRRLIB_SetFBMode (0);
	
	return page;
	}

static bool QuerySelection (int ai)
	{
	int i;
	float mag = 1.0;
	int spot = -1;
	int incX = 1, incY = 1;
	int y = 220;
	int yInf = 490;
	
	for (i = 0; i < gui.spotsIdx; i++)
		{
		if (ai == gui.spots[i].id)
			spot = i;
		}
	
	if (spot < 0) return false;

	s_grlib_icon ico;
	grlib_IconInit (&ico, &gui.spots[spot].ico);
	ico.sel = true;
	*ico.title = '\0';
	
	s_grlib_iconSetting istemp;
	memcpy (&istemp, &is, sizeof(s_grlib_iconSetting));
	
	s_grlibobj black;
	black.x1 = 0;
	black.y1 = 0;
	black.x2 = grlib_GetScreenW();
	black.y2 = grlib_GetScreenH();
	black.color = RGBA(0,0,0,192);
	black.bcolor = RGBA(0,0,0,192);
	
	Video_SetFont(TTFNORM);
	
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

		DrawInfoWindo (yInf, fsop_GetFilename(emus[emuSelected].name, true), "Press (A) to start, (B) Cancel");
		yInf -= 5;
		if (yInf < 400) yInf = 400;

		Overlay ();
		grlib_DrawIRCursor ();
		grlib_Render();
		
		if (mag < 3.0) mag += 0.1;
		if (mag >= 3.0 && ico.x == 320 && ico.y == y) break;
		}
	
	u32 btn;
	while (true)
		{
		grlib_PopScreen ();
		grlib_DrawSquare (&black);
		grlib_IconDraw (&istemp, &ico);
		Overlay ();
		
		DrawInfoWindo (yInf, fsop_GetFilename(emus[emuSelected].name, true), "Press (A) to start, (B) Cancel");
		yInf -= 5;
		if (yInf < 400) yInf = 400;
		
		grlib_DrawIRCursor ();
		grlib_Render();

		btn = grlib_GetUserInput();
		if (btn & WPAD_BUTTON_A) return true;
		if (btn & WPAD_BUTTON_B) return false;
		}
	return true;
	}

static void StartEmu (int type, char *fullpath)
	{
	char fn[128];
	char path[128];
	char cmd[256];
	char dol[256];
	char buff[256];
	char *p;
	
	strcpy (buff, fullpath);
	
	int i;
	
	// remove extension
	i = strlen(buff)-1;
	while (i > 0 && buff[i] != '/')
		{
		i--;
		}
	if (buff[i] == '/') buff[i] = 0;
	strcpy (fn, &buff[i+1]);
	strcpy (path, buff);
	strcat (path, "/");
	
	p = strstr (path, "//");
	if (p) 
		p += 2;
	else
		p = path;

	sprintf (cmd, "%s;%s;%s://ploader/plugins/forwarder.dol;00010001;504F5354;postLoader", path, fn, vars.defMount);
	sprintf (dol, "%s://ploader/plugins/%s", vars.defMount, Plugins_Get(type, PIN_DOL));
	
	Debug ("StartEmu %s (%s)", dol, cmd);

	if (fsop_FileExist (dol))
		{
		Conf (false);	// Store configuration on disc
		Plugins (false);

		DirectDolBoot (dol, cmd, 0);
		}
	else
		{
		grlib_menu (0, "Attention!", "Requested plugin not found\n\n'%s'", dol);
		}
	}
			
int EmuBrowser (void)
	{
	u32 btn;

	Debug ("GameBrowser (begin)");
	
	Plugins (true);
	Conf (true);

	scanned = 0;
	browserRet = -1;

	grlibSettings.color_window = RGBA(192,192,192,255);
	grlibSettings.color_windowBg = RGBA(32,32,32,128);
	
	grlib_SetRedrawCallback (Redraw, Overlay);
	
	usedBytes = 0;
	emus = calloc (EMUMAX, sizeof(s_emu));
	
	// Immediately draw the screen...
	StructFree ();
	InitializeGui ();
	
	/*
	Redraw ();
	grlib_PushScreen ();
	grlib_PopScreen ();
	grlib_Render();  // Render the theme.frame buffer to the TV
	*/
	
	page = config.gamePageEmu;
	EmuBrowse ();
	
	LiveCheck (1);

	redraw = 1;
	
	// Loop forever
    while (browserRet == -1) 
		{
		if (LiveCheck (0)) redraw = 1;
		
		btn = grlib_GetUserInput();
		
		// If [HOME] was pressed on the first Wiimote, break out of the loop
		if (btn)
			{
			browserRet = ChooseDPadReturnMode (btn);
			if (browserRet != -1) btn = 0;
			
			if (btn & WPAD_BUTTON_A && emuSelected != -1) 
				{
				if (!QuerySelection (emuSelected))
					{
					redraw = 1;
					continue;
					}
					
				Debug ("emuRun");
				ReadGameConfig (emuSelected);
				emus[emuSelected].playcount++;
				WriteGameConfig (emuSelected);
				config.gamePageEmu = page;
				
				StartEmu (emus[emuSelected].type, emus[emuSelected].name);

				redraw = 1;
				}
				
			/////////////////////////////////////////////////////////////////////////////////////////////
			// Select application as default
			if (btn & WPAD_BUTTON_B && emuSelected != -1)
				{
				ShowAppMenu (emuSelected);
				redraw = 1;
				}

			if (btn & WPAD_BUTTON_1) 
				{
				if (emusCnt == 0) continue;

				page = GoToPage (page, pageMax);
				FeedCoverCache ();
				redraw = 1;
				}
			
			if (btn & WPAD_BUTTON_2)
				{
				if (emusCnt == 0) continue;
				
				ShowFilterMenu ();
				redraw = 1;
				}
				
			if (btn & WPAD_BUTTON_HOME)
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
		
		if (CoverCache_IsUpdated ()) redraw = 1;
		
		if (redraw)
			{
			Redraw ();
			grlib_PushScreen ();
			redraw = 0;
			}
		
		REDRAW();
		
		if (browse)
			{
			browse = 0;
			EmuBrowse ();
			redraw = 1;
			}
		
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
			browserRet = INTERACTIVE_RET_TOGAMES;
			}
		}
		
	// Lets close the topbar, if needed
	CLOSETOPBAR();
	CLOSEBOTTOMBAR();
	
	config.gamePageEmu = page;
	
	Conf (false);
	Plugins (false);

	// Clean up all data
	StructFree ();
	gui_Clean ();
	free (emus);
	
	grlib_SetRedrawCallback (NULL, NULL);
	
	return browserRet;
	}