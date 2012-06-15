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

#define CHNMAX 4096

/*

This allow to browse for applications in emus folder of mounted drive

*/
extern s_grlibSettings grlibSettings;
static s_emuConfig emuConf;

static int browse = 0;
static int scanned = 0;

#define CATN 4
#define CATMAX 4

static char *flt[CATN] = { 
"SNES: Super Nintendo Entertainment System",
"NES: Nintendo Entertainment System",
"VBA: Visual Boy Advance",
"GENESIS: Sega Mega Drive/Genesis"
};

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

static int refreshPng = 0;

static s_grlib_iconSetting is;

static void Redraw (void);
static int EmuBrowse (void);
static bool IsFiltered (int ai);

static int disableSpots;

static s_cfg *cfg;

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

/*
This will extract the pathname from full rom path
*/
static char * GetFilename (char *path)
	{
	static char fn[256];
	char buff[256];
	
	strcpy (buff, path);
	
	int i;
	
	// remove extension
	i = strlen(buff)-1;
	while (i > 0 && buff[i] != '.')
		{
		i--;
		}
	if (buff[i] == '.') buff[i] = 0;

	i = strlen(buff)-1;
	while (i > 0 && buff[i] != '/')
		{
		i--;
		}
	strcpy (fn, &buff[i+1]);
	
	return fn;
	}
	
static char * GetPath (char *path)
	{
	static char fn[256];
	
	strcpy (fn, path);
	
	int i;
	
	i = strlen(fn)-1;
	while (i > 0 && fn[i] != '/')
		{
		i--;
		}
	fn[i] = 0;
	
	return fn;
	}
	
static void MakeCoverPath (int ai, char *path)
	{
	sprintf (path, "%s://ploader/covers.emu/%s.png", vars.defMount, GetFilename(emus[ai].name));
	//grlib_dosm (path);
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
		
		if (ai >= 0 && ai < emusCnt)
			{
			MakeCoverPath (ai, path);
			CoverCache_Add (path, (i >= 0 && i < gui.spotsXpage) ? true:false );
			}
		}
	
	CoverCache_Pause (false);
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
	cfg_SetString (cfg, GetFilename(emus[ia].name), buff);
	free (buff);
	}

static int ReadGameConfig (int ia)
	{
	char buff[1024];
	bool valid;
	
	valid = cfg_GetString (cfg, GetFilename(emus[ia].name), buff);
	
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
				strcpy (romname, GetFilename (emus[i].name));
				romname[32] = 0;
				Video_WaitPanel (TEX_HGL, "Searching...|(B) Stop", romname, emusCnt);
				t = time(NULL)+1;
				}

			strcpy (romname, GetFilename (emus[i].name));
			
			if (ms_strstr (pent->d_name, romname))
				{
				sprintf (fn, "%s/%s", path, pent->d_name);
				
				size_t size;
				fsop_GetFileSizeBytes (fn, &size);
				
				if (size > 4096)
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
			}
		}
	}

////////////////////////////////////////////////////////////////////////////////

static int qsort_name (const void * a, const void * b)
	{
	s_emu *aa = (s_emu*) a;
	s_emu *bb = (s_emu*) b;
	return (ms_strcmp (aa->name, bb->name));
	}
	
static int qsort_filter (const void * a, const void * b)
	{
	s_emu *aa = (s_emu*) a;
	s_emu *bb = (s_emu*) b;

	if (aa->filtered > bb->filtered) return -1;
	if (aa->filtered < bb->filtered) return 1;
	
	return 0;
	}
	
static void AppsSort (void)
	{
	Debug ("AppsSort: begin");
	
	int i;
	
	// Apply filters
	emus2Disp = 0;
	for (i = 0; i < emusCnt; i++)
		{
		emus[i].filtered = IsFiltered (i);
		if (emus[i].filtered && (!emus[i].hidden || showHidden)) emus2Disp++;
		}
	
	Video_WaitPanel (TEX_HGL, "Please wait...|Sorting...");
	qsort (emus, emusCnt, sizeof(s_emu), qsort_filter);

	Video_WaitPanel (TEX_HGL, "Please wait...|Sorting...");
	qsort (emus, emus2Disp, sizeof(s_emu), qsort_name);

	pageMax = (emus2Disp-1) / gui.spotsXpage;
	refreshPng = 1;
	
	Debug ("AppsSort: end");
	}
	
static int BrowseRomFolder (int type, int startidx, char *path)
	{
	int i = startidx;
	DIR *pdir;
	struct dirent *pent;
	char fn[300];
	int updater = 0;

	pdir=opendir(path);

	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0)
			continue;
		
		sprintf (fn, "%s/%s", path, pent->d_name);
		emus[i].name = calloc (1, strlen (fn) + 1);
		
		if (!emus[i].name)
			{
			Debug ("allocation error (%s)", pent->d_name);
			continue;
			}
		strcpy (emus[i].name, fn);
		
		emus[i].type = type;
		
		if (++updater > 100)
			{
			Video_WaitPanel (TEX_HGL, "Please wait...|Searching for roms...");
			updater = 0;
			}
		
		i++;
		}
		
	return i-startidx;
	}
	
static int EmuBrowse (void)
	{
	int i;
	Debug ("begin EmuBrowse");
	
	gui.spotsIdx = 0;
	gui_Clean ();
	StructFree ();

	Video_WaitPanel (TEX_HGL, "Please wait...");

	
	char path[300];
	int dev;
	
	Debug ("Emu Browse: searching for snes9xgx roms");
	
	emusCnt = 0;
	for (dev = 0; dev < DEV_MAX; dev++)
		{
		if (devices_Get (dev))
			{
			Debug ("Checking: %s", devices_Get (dev));
			
			// SNES/Superfamicon
			sprintf (path, "%s://snes9xgx/roms", devices_Get (dev));
			i = BrowseRomFolder (EMU_SNES, emusCnt, path);
			emusCnt += i;
			
			Debug ("found %d roms in %s", i, path);

			// NES/Famicom
			sprintf (path, "%s://fceugx/roms", devices_Get (dev));
			i = BrowseRomFolder (EMU_NES, emusCnt, path);
			emusCnt += i;
			
			Debug ("found %d roms in %s", i, path);

			// Game Boy/Game Boy Advance Emulator
			sprintf (path, "%s://vbagx/roms", devices_Get (dev));
			i = BrowseRomFolder (EMU_VBA, emusCnt, path);
			emusCnt += i;
			
			Debug ("found %d roms in %s", i, path);

			// Genesis Emulator
			sprintf (path, "%s://genplus/roms", devices_Get (dev));
			i = BrowseRomFolder (EMU_GEN, emusCnt, path);
			emusCnt += i;
			
			Debug ("found %d roms in %s", i, path);
			}
		}
			
	scanned = 1;
	
	Debug ("end EmuBrowse");

	AppsSort ();
	
	Debug ("FeedCoverCache");
	FeedCoverCache ();
	Debug ("FeedCoverCache (end)");

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
			Video_SetFont(TTFNORM);
			
			emuSelected = gui.spots[i].id;

			gui.spots[i].ico.sel = true;
			grlib_IconDraw (&is, &gui.spots[i].ico);

			if (emus[emuSelected].type == EMU_SNES) sprintf (buff, "SNES: %s", GetFilename (emus[emuSelected].name));
			if (emus[emuSelected].type == EMU_NES) sprintf (buff, "NES: %s", GetFilename (emus[emuSelected].name));
			if (emus[emuSelected].type == EMU_VBA) sprintf (buff, "VBA: %s", GetFilename (emus[emuSelected].name));
			if (emus[emuSelected].type == EMU_GEN) sprintf (buff, "GEN: %s", GetFilename (emus[emuSelected].name));
			
			Video_SetFont(TTFNORM);
			char title[256];
			strcpy (title, buff);
			if (strlen (title) > 48)
				{
				title[48] = 0;
				strcat (title, "...");
				}
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, title);

			Video_SetFont(TTFSMALL);
			grlib_printf (XMIDLEINFO, theme.line1Y, GRLIB_ALIGNCENTER, 0, GetPath(emus[emuSelected].name));
			
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
	int i,j;
	bool ret = false;
	char f[128];
	//char name[256];
	
	if (config.emuFilter == 0) return true;
	
	memset (f, 0, sizeof(f));

	j = 0;
	for (i = 0; i < CATMAX; i++)
		{
		f[j++] = (config.emuFilter & (1 << i)) ? '1':'0';
		//f[j++] = (emus[ai].type & (1 << i)) ? '1':'0';
		//f[j++] = ' ';
		
		if ((config.emuFilter & (1 << i)) && (emus[ai].type == i))
			{
			ret = true;
			}
		}
	f[j++] = 0x0;
	
	//sprintf (name, emus[ai].name);
	//name[32] = 0;
	//Debug ("%s %s %d %d = %d", name, f, emus[ai].type, ai, ret);
		
	return ret;
	}
	
static void ShowFilterMenu (void)
	{
	char buff[512];
	u8 f[CATN];
	int i, item;

	for (i = 0; i <CATN; i++)
		f[i] = 0;
	
	for (i = 0; i < CATN; i++)
		f[i] = (config.emuFilter & (1 << i)) ? 1:0;

	do
		{
		buff[0] = '\0';
		for (i = 0; i < CATMAX; i++)
			{
			if (i == 8 || i == 16 || i == 24) grlib_menuAddColumn (buff);
			grlib_menuAddCheckItem (buff, 100 + i, f[i], flt[i]);
			}
		
		item = grlib_menu ("Filter menu\nPress (B) to close, (+) Select all, (-) Deselect all (shown all emus)", buff);

		if (item == MNUBTN_PLUS)
			{
			int i; 	for (i = 0; i < CATN; i++) f[i] = 1;
			}

		if (item == MNUBTN_MINUS)
			{
			int i; 	for (i = 0; i < CATN; i++) f[i] = 0;
			}
		
		if (item >= 100)
			{
			int i = item - 100;
			f[i] = !f[i];
			}
		}
	while (item != -1);
	
	config.emuFilter = 0;
	for (i = 0; i < CATN; i++)
		if (f[i]) config.emuFilter |= (1 << i);
	
	EmuBrowse ();
	AppsSort ();
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
		grlib_menuAddSeparator (buff);
		grlib_menuAddItem (buff,  2, "Delete this rom (and cover)");
		grlib_menuAddItem (buff,  3, "Delete this cover");
		
		grlibSettings.fontNormBMF = fonts[FNTBIG];
		int item = grlib_menu (title, buff);
		grlibSettings.fontNormBMF = fonts[FNTNORM];
		
		if (item == 1)
			{
			int item = grlib_menu ("Reset play count ?", "Yes~No");
			
			if (item == 0)
				emus[ai].playcount = 0;
			
			WriteGameConfig (ai);
			AppsSort ();
			}
			
		if (item == 2)
			{
			sprintf (buff, "Are you sure you want to delete this ROM and it's cover ?\n\n%s", title);
			int item = grlib_menu (buff, "Yes~No");
			
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
			int item = grlib_menu (buff, "Yes~No");
			
			if (item == 0)
				{
				char cover[256];
				MakeCoverPath (ai, cover);
				
				unlink (cover);

				EmuBrowse();
				}
			}
		break;
		}
	while (TRUE);

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

	if (showHidden)
		grlib_menuAddItem (buff,  6, "Hide hidden emus");
	else
		grlib_menuAddItem (buff,  7, "Show hidden emus");
		
	grlib_menuAddItem (buff,  1, "Import snapshots as covers");

	Redraw();
	grlib_PushScreen();
	
	grlibSettings.fontNormBMF = fonts[FNTBIG];
	int item = grlib_menu ("Games menu", buff);
	grlibSettings.fontNormBMF = fonts[FNTNORM];
	
	if (item == 1)
		{
		GetCovers ();
		goto start;
		}

	if (item == 9)
		{
		config.gameSort ++;
		if (config.gameSort > 2) config.gameSort = 0;
		AppsSort ();
		FeedCoverCache ();
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
		AppsSort ();
		}

	if (item == 7)
		{
		showHidden = 1;
		AppsSort ();
		}
		
	}

static void RedrawIcons (int xoff, int yoff)
	{
	int i;
	int ai;	// Application index (corrected by the offset)
	char path[256];
	
	Video_SetFont(TTFNORM);

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
				char title[256];
				strcpy (title, GetFilename(emus[ai].name));
				title[48] = 0;
				strcpy (gui.spots[gui.spotsIdx].ico.title, title);
				}
			else
				gui.spots[gui.spotsIdx].ico.title[0] = '\0';

			// Is it hidden ?
			if (emus[ai].hidden && showHidden)
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = vars.tex[TEX_GHOST];
				
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

	if (emusCnt == 0)
		{
		grlib_DrawCenteredWindow ("No emus found, rebuild cache!", WAITPANWIDTH, 133, 0, NULL);
		Video_DrawIcon (TEX_EXCL, 320, 250);
		}
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
		grlib_DrawCenteredWindow ("No roms found !", WAITPANWIDTH, 133, 0, NULL);
		Video_DrawIcon (TEX_EXCL, 320, 250);
		}
		
	refreshPng = 0;
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
			grlib_GetUserInput();
			grlib_DrawIRCursor ();
			grlib_Render();
			
			usleep (1);
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
			grlib_GetUserInput();
			grlib_DrawIRCursor ();
			grlib_Render();
			
			usleep (1);
			}
		while (x < 640);
		}
	
	redrawIcons = true;
	redraw = 1;
	
	return page;
	}

static bool QuerySelection (int ai)
	{
	int i;
	float mag = 1.0;
	int spot = -1;
	int incX = 1, incY = 1;
	int y = 220;
	
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

		Overlay ();
		grlib_GetUserInput();
		grlib_DrawIRCursor ();
		grlib_Render();
		
		if (mag < 2.3) mag += 0.05;
		if (mag >= 2.3 && ico.x == 320 && ico.y == y) break;
		}
	
	int fr = grlibSettings.fontReverse;
	u32 btn;
	while (true)
		{
		grlib_PopScreen ();
		grlib_DrawSquare (&black);
		grlib_IconDraw (&istemp, &ico);
		Overlay ();
		
		grlibSettings.fontReverse = 0;
		grlib_printf (XMIDLEINFO, theme.line1Y, GRLIB_ALIGNCENTER, 0, emus[ai].name);		
		grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "Press (A) to start, (B) Cancel");
		grlibSettings.fontReverse = fr;
		
		grlib_GetUserInput();
		grlib_DrawIRCursor ();
		grlib_Render();

		btn = grlib_GetUserInput();
		if (btn & WPAD_BUTTON_A) return true;
		if (btn & WPAD_BUTTON_B) return false;
		}
	return true;
	}

static void Conf (bool open)
	{
	char cfgpath[64];
	sprintf (cfgpath, "%s://ploader/emus.conf", vars.defMount);

	if (open)
		{
		cfg = cfg_Alloc (cfgpath, 0);
		}
	else
		{
		cfg_Store (cfg, cfgpath);
		cfg_Free (cfg);
		}
	}
	
static void StartEmu (int type, char *fullpath)
	{
	char fn[128];
	char path[128];
	char cmd[256];
	char dol[256];
	char buff[256];
	
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
	
	sprintf (cmd, "%s;%s;%s://ploader/plugins/forwarder.dol;00010001;504F5354;postLoader", path, fn, vars.defMount);
	if (type == EMU_SNES)
		sprintf (dol, "%s://ploader/plugins/snes9xgx.dol", vars.defMount);
	if (type == EMU_NES)
		sprintf (dol, "%s://ploader/plugins/fceugx.dol", vars.defMount);
	if (type == EMU_VBA)
		sprintf (dol, "%s://ploader/plugins/vbagx.dol", vars.defMount);
	if (type == EMU_GEN)
		sprintf (dol, "%s://ploader/plugins/genplusgx.dol", vars.defMount);

	if (fsop_FileExist (dol))
		{
		DirectDolBoot (dol, cmd, 0);
		}
	else
		{
		grlib_menu ("Attention!", "Requested plugin not found\n\n'%s'", dol);
		}
	}
			
int EmuBrowser (void)
	{
	u32 btn;

	Debug ("GameBrowser (begin)");
	
	Conf (true);

	scanned = 0;
	browserRet = -1;

	grlibSettings.color_window = RGBA(192,192,192,255);
	grlibSettings.color_windowBg = RGBA(32,32,32,128);
	
	grlib_SetRedrawCallback (Redraw, Overlay);
	
	emus = calloc (CHNMAX, sizeof(s_game));
	
	// Immediately draw the screen...
	StructFree ();
	InitializeGui ();
	
	Redraw ();
	grlib_PushScreen ();
	grlib_PopScreen ();
	grlib_Render();  // Render the theme.frame buffer to the TV
	
	page = config.gamePageWii;
	EmuBrowse ();

	LiveCheck (1);

	page = config.gamePageEmu;

	FeedCoverCache ();

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
			if (browserRet != -1) break;
			
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
				Conf (false);	// Store configuration on disc
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
		
		if (grlibSettings.wiiswitch_poweroff || grlibSettings.wiiswitch_reset)
			{
			browserRet = INTERACTIVE_RET_SHUTDOWN;
			break;
			}

		if (wiiload.status == WIILOAD_HBZREADY)
			{
			WiiloadZipMenu ();
			redraw = 1;
			}
			
		if (wiiload.status == WIILOAD_HBREADY)
			{
			if (WiiloadPostloaderDolMenu())
				browserRet = INTERACTIVE_RET_WIILOAD;
			else
				redraw = 1;
			break;
			}
			
		if (vars.themeReloaded) // Restart the browser
			{
			vars.themeReloaded = 0;
			browserRet = INTERACTIVE_RET_TOGAMES;
			}
		
		usleep (5000);
		}
		
	// Lets close the topbar, if needed
	CLOSETOPBAR();
	
	config.gamePageEmu = page;
	
	Conf (false);

	// Clean up all data
	StructFree ();
	gui_Clean ();
	free (emus);
	
	grlib_SetRedrawCallback (NULL, NULL);
	
	return browserRet;
	}