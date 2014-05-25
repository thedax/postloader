/*

This allow to browse for applications in emus folder of mounted drive

*/
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <dirent.h>
#include <ctype.h>
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
#include "bits.h"

#define EMUMAX 32768
#define EMUPINMAX 256
#define ROMPATHMAX 1024

typedef struct
	{
	u8 enabled;
	u8 enabledCfg;
	u8 recurse;
	u16 count;
	char *description;
	char *id;
	char *romfolder;
	char *ext;
	char *icon;
	}
s_pin;

typedef struct
	{
	char *id;
	char *dol;
	char *args;
	}
s_emudol;

typedef struct
	{
	unsigned int alreadyAdded;
	unsigned int noMoreFolderSpace;
	unsigned int allocErrors;
	}
s_browseflag;

typedef struct
	{
	s_pin pin[EMUPINMAX];
	int count;
	char *rompaths[ROMPATHMAX];
	int rompathscount;

	char *dolpath;
	s_emudol emudols[128];
	int eumdolscount;
	}
s_plugin;
	
extern s_grlibSettings grlibSettings;

static int rescanRoms = 0;
static int browse = 0;
static int scanned = 0;

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
static int EmuBrowse (bool rebuild);
static bool IsFiltered (int ai);

static int disableSpots;

//static s_cfg *cfg;
static s_plugin plugin;
static s_browseflag browseflag;

static GRRLIB_texImg **emuicons;

static int usedBytes = 0;

#define ICONW 100
#define ICONH 93

#define XMIDLEINFO 320
#define INIT_X (30 + ICONW / 2)
#define INIT_Y (60 + ICONH / 2)
#define INC_X ICONW+22
#define INC_Y ICONH+25

static void MakeCoverPath (int ai, char *path);
static void FixFilters (void);
static char* plugin_GetRomFullPath (int idx);

static void plugin_Init (void)
	{
	memset (&plugin, 0, sizeof(plugin));
	}
	
static int plugin_AssignRomPath (char *fullpath, int idx)
	{
	if (browseflag.noMoreFolderSpace) return 0;
	
	int i;
	char filename[256];
	char path[256];
	
	// these three lines must be erased when per-game configuration is back
	emus[idx].priority = 5;
	emus[idx].hidden = 0;
	emus[idx].playcount = 0;
	
	/*
	for (i = 0; i < emusCnt; i++)
		{
		if (ms_isequal (fullpath, plugin_GetRomFullPath(i)))
			{
			browseflag.alreadyAdded++;
			return 0;
			}
		}
	*/
	
	strcpy (filename, fsop_GetFilename(fullpath, 0));
	strcpy (path, fsop_GetPath(fullpath, 0));
	
	for (i = 0; i < plugin.rompathscount; i++)
		{
		if (strcmp (plugin.rompaths[i], path) == 0)
			{
			emus[idx].name = malloc (strlen(filename)+1);
			strcpy (emus[idx].name, filename);
			
			emus[idx].pathid = i;
			return 2;
			}
		}
	
	if (plugin.rompathscount >= ROMPATHMAX-1)
		{
		browseflag.noMoreFolderSpace++;
		Debug ("plugin_AssignRomPath: %d limit reached (%s)", ROMPATHMAX, filename);

		return 0;
		}
		
	plugin.rompaths[plugin.rompathscount] = malloc (strlen(path)+1);
	strcpy (plugin.rompaths[plugin.rompathscount], path);
	emus[idx].pathid = plugin.rompathscount;
	plugin.rompathscount++;

	emus[idx].name = malloc (strlen(filename)+1);
	if (!emus[idx].name)
		{
		browseflag.allocErrors ++;
		Debug ("plugin_AssignRomPath: allocation error (%s)", filename);
		return 0;
		}
	strcpy (emus[idx].name, filename);
	
	return 1;
	}

static int plugin_AddRomPath (char *path)
	{
	int i;
	for (i = 0; i < plugin.rompathscount; i++)
		{
		if (strcmp (plugin.rompaths[i], path) == 0)
			{
			return 2; // already added
			}
		}
	if (plugin.rompathscount >= ROMPATHMAX-1)
		{
		return 0; // no mor space
		}
		
	// add-it
	plugin.rompaths[plugin.rompathscount] = malloc (strlen(path)+1);
	strcpy (plugin.rompaths[plugin.rompathscount], path);
	plugin.rompathscount++;

	return 1;
	}

static char* plugin_GetRomFullPath (int idx)
	{
	static char fullpath[256];
	
	if (emus[idx].pathid < 0)
		strcpy (fullpath, emus[idx].name);
	else
		sprintf (fullpath, "%s/%s", plugin.rompaths[emus[idx].pathid], emus[idx].name);

	return fullpath;
	}

static void plugin_FreeRompaths (void)
	{
	int i;

	for (i = 0; i < plugin.rompathscount; i++)
		{
		if (plugin.rompaths[i])
			free (plugin.rompaths[i]);
			
		plugin.rompaths[i] = NULL;
		}
		
	plugin.rompathscount = 0;
	}
	
static void plugin_Free (void)
	{
	int i;
	
	for (i = 0; i < EMUPINMAX; i++)
		{
		s_pin *pin = &plugin.pin[i];
		
		if (pin->description) free (pin->description);
		if (pin->id) free (pin->id);
		if (pin->romfolder) free (pin->romfolder);
		if (pin->ext) free (pin->ext);
		if (pin->icon) free (pin->icon);
		}
	
	plugin_FreeRompaths ();

	if (plugin.dolpath) free (plugin.dolpath);
	
	plugin_Init ();
	}

static bool plugin_Set (int i, u8 enabled, u8 recurse, char *description, char *dol, char *romfolder, char *ext, char *icon)
	{
	if (!description || !dol || !romfolder || !ext)
		return false;
	
	s_pin *pin = &plugin.pin[i];

	if (pin->description) free (pin->description);
	if (pin->id) free (pin->id);
	if (pin->romfolder) free (pin->romfolder);
	if (pin->ext) free (pin->ext);
	if (pin->icon) free (pin->icon);
	
	pin->enabledCfg = enabled;
	pin->enabled = enabled;
	pin->recurse = recurse;
	pin->description = description;
	pin->id = dol;
	pin->romfolder = romfolder;
	pin->ext = ext;
	pin->icon = icon;
	pin->description = description;
	
	/*
	Debug ("%d:%d:%d:%s:%s:%s:%s:%s:%s", 
		i,
		pin->enabled,
		pin->recurse,
		pin->description,
		pin->id,
		pin->romfolder,
		pin->ext,
		pin->icon,
		pin->args);
	*/
	
	return true;
	}
	
static void retroarch_PurgeBmps (void)
	{
	char path[300];
	int dev;
	
	if (!config.enableRetroarchAutocover)
		{
		Debug ("retroarch_PurgeBmps: disabled");
		return;
		}
	
	Debug ("retroarch_PurgeBmps: begin");

	for (dev = 0; dev < DEV_MAX; dev++)
		{
		if (devices_Get (dev))
			{
			sprintf (path, "%s:/retroarch", devices_Get (dev));
			
			//mt_Lock();
			char *buff = fsop_GetDirAsString (path, '\0', 1, ".bmp");
			if (buff)
				{
				char *p = buff;
				
				while (*p != '\0')
					{
					sprintf (path, "%s:/retroarch/%s", devices_Get (dev), p);
					Debug (path);
					unlink (path);
					p += strlen(p)+1;
					}
				
				free (buff);
				}
			//mt_Unlock();
			}
		}
	
	Debug ("retroarch_PurgeBmps: end");
	}

static void retroarch_GetCover (void)
	{
	char path[300];
	int dev;
	
	if (!config.enableRetroarchAutocover || !*config.lastRom)
		{
		Debug ("retroarch_GetCover: disabled or no rom info...");
		return;
		}
	
	Debug ("retroarch_GetCover: begin");

	for (dev = 0; dev < DEV_MAX; dev++)
		{
		if (devices_Get (dev))
			{
			sprintf (path, "%s:/retroarch", devices_Get (dev));
			
			//mt_Lock();
			char *buff = fsop_GetDirAsString (path, '\0', 1, ".bmp");
			if (buff) // we have a rom snapshot!
				{
				sprintf (path, "%s:/retroarch/%s", devices_Get (dev), buff);
				Debug (path);

				size_t size;
				fsop_GetFileSizeBytes (path, &size);
				
				if (size > 128)
					{
					char target[300];
					
					sprintf (target, "%s/%s.%s", vars.coversEmu, fsop_GetFilename(config.lastRom, true), fsop_GetExtension(buff));
					fsop_CopyFile (path, target, 0);
					Debug ("retroarch_GetCover %s -> %s", path, target);
					}
				else
					{
					Debug ("retroarch_GetCover %s is to small", path);
					}
				}
			//mt_Unlock();
			}
		}
	
	Debug ("retroarch_GetCover: end");
	
	retroarch_PurgeBmps ();
	}

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
	/*
	Debug ("Conf %d (begin))", open);
	
	char cfgpath[64];
	sprintf (cfgpath, "%s://ploader/emus.conf", vars.defMount);

	Video_WaitIcon (TEX_HGL);
	
	cfg_Section (NULL);
	if (open)
		{
		mt_Lock();
		cfg = cfg_Alloc (cfgpath, 16384, 0, 0);
		mt_Unlock();
		}
	else
		{
		mt_Lock();		
		cfg_Store (cfg, cfgpath);
		mt_Unlock();
		cfg_Free (cfg);
		}

	Debug ("Conf %d (end))", open);
	*/
	}
	
static void WriteGameConfig (int ia)
	{
	/*
	Debug ("WriteGameConfig: begin");
	if (ia < 0) return;
	
	char buff[2048];
	int index = 0;
	*buff = '\0';
	
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, &emus[ia].priority, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, &emus[ia].hidden, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U16, &emus[ia].playcount, index++);

	cfg_SetString (cfg, fsop_GetFilename(emus[ia].name, true), buff);
	
	Debug ("WriteGameConfig: end");
	*/
	}

static int ReadGameConfig (int ia)
	{
	/*
	Debug ("ReadGameConfig: begin");
	char buff[2048];
	int index = 0;
	*buff = '\0';
	
	if (cfg_GetString (cfg, fsop_GetFilename(emus[ia].name, true), buff) >= 0 && strlen(buff) < 250) // old format
		{
		cfg_FmtString (buff, CFG_READ, CFG_U8, &emus[ia].priority, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, &emus[ia].hidden, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U16, &emus[ia].playcount, index++);
		}
	else
		{
		emus[ia].priority = 5;
		emus[ia].hidden = 0;
		emus[ia].playcount = 0;
		}
	
	Debug ("ReadGameConfig: end");
	*/
	
	return true;
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static void Plugins (bool open)
	{
	Debug ("Plugins %d (begin))", open);

	char cfgpath[64];
	char path[1024];
	int i;
		
	sprintf (cfgpath, "%s://ploader/plugins.conf", vars.defMount);

	if (open)
		{
		s_cfg *pcfg;

		plugin_Free ();
		
		//mt_Lock();
		pcfg = cfg_Alloc (cfgpath, 384, 0, 1);
		//mt_Unlock();
		
		if (!pcfg)
			{
			Debug ("Plugins: can't open or allocate plugins.conf");
			return;
			}
			
		Debug ("pcfg->count: %d", pcfg->count);

		plugin.count = 0;

		for (i = 0; i < pcfg->count; i++)
			{
			// Rename the plugin
			if (strcmp (pcfg->tags[i], "dol") == 0)
				{
				plugin.emudols[plugin.eumdolscount].id = ms_GetDelimitedString (pcfg->items[i], '|', 0);
				plugin.emudols[plugin.eumdolscount].dol = ms_GetDelimitedString (pcfg->items[i], '|', 1);
				plugin.emudols[plugin.eumdolscount].args = ms_GetDelimitedString (pcfg->items[i], '|', 2);
				
				Debug ("%d:%s:%s:%s", plugin.eumdolscount, plugin.emudols[plugin.eumdolscount].id, plugin.emudols[plugin.eumdolscount].dol, plugin.emudols[plugin.eumdolscount].args);
				plugin.eumdolscount++;
				}
			// Rename the plugin
			if (strcmp (pcfg->tags[i], "plugin") == 0 || pcfg->tags[i][0] == '0')
				{
				if (plugin_Set (plugin.count, 
						atoi(ms_GetDelimitedString (pcfg->items[i], '|', 0)),
						atoi(ms_GetDelimitedString (pcfg->items[i], '|', 1)),
						ms_GetDelimitedString (pcfg->items[i], '|', 2),
						ms_GetDelimitedString (pcfg->items[i], '|', 3),
						ms_GetDelimitedString (pcfg->items[i], '|', 4),
						ms_GetDelimitedString (pcfg->items[i], '|', 5),
						ms_GetDelimitedString (pcfg->items[i], '|', 6)
						)
					)
					plugin.count++;
				}
			
			if (plugin.count >= EMUPINMAX) break;
			}
			
		Debug ("Plugins: %d", plugin.count);
		
		cfg_Value (pcfg, CFG_READ, CFG_STRING, "dolpath", path, -1);
		if (*path)
			{
			plugin.dolpath = malloc (strlen(path)+1);
			strcpy (plugin.dolpath, path);
			}

		cfg_Free (pcfg);

		emuicons = calloc (sizeof (GRRLIB_texImg), plugin.count);
		Video_PanelsUpdateTime (100);
		for (i = 0; i < plugin.count; i++)
			{
			Video_WaitIcon (TEX_HGL);
			
			sprintf (path, "%s://ploader/theme/%s", vars.defMount, plugin.pin[i].icon);
			emuicons[i] = GRRLIB_LoadTextureFromFile (path);

			if (!emuicons[i])
				{
				sprintf (path, "%s://ploader/plugins/%s", vars.defMount, plugin.pin[i].icon);
				emuicons[i] = GRRLIB_LoadTextureFromFile (path);
				}
			}
		Video_PanelsUpdateTime (0);
		}
	else
		{
		int i;
		
		for (i = 0; i < plugin.count; i++)
			GRRLIB_FreeTexture (emuicons[i]);
		
		free (emuicons);
		plugin_Free ();
		}

	Debug ("Plugins %d (end)", open);
	}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int CountRomsPlugin (int index)
	{
	int i;
	int count = 0;
	
	for (i = 0; i < emusCnt; i++)
		{
		if (emus[i].type == index) count++;
		}

	return count;
	}
	
static void MakeCoverPath (int ai, char *path)
	{
	*path = '\0';
	
	if (!emus[ai].cover) return;
	if (!strstr (emus[ai].cover,".")) // packed name
		{
		sprintf (path, "%s/%s.%s", vars.coversEmu, fsop_GetFilename(emus[ai].name,true), emus[ai].cover);
		//Debug (">>>>> %s", path);
		}
	else
		{
		sprintf (path, "%s/%s", vars.coversEmu, emus[ai].cover);
		}
	}

static void FeedCoverCache (void)
	{
	char path[256];
	
	CoverCache_Lock ();

	if (page > pageMax) page = pageMax;
	if (page < 0) page = 0;

	int i;
	int ai;	// Application index (corrected by the offset)

	for (i = -gui.spotsXpage; i < gui.spotsXpage*2; i++)
		{
		ai = (page * gui.spotsXpage) + i;
		
		if (ai >= 0 && ai < emusCnt && emus[ai].cover)
			{
			MakeCoverPath (ai, path);
			CoverCache_Add (path, (i >= 0 && i < gui.spotsXpage) ? true:false );
			}
		}
	
	CoverCache_Unlock ();
	}

static void StructFree (void)
	{
	int i;
	
	for (i = 0; i < emusCnt; i++)
		{
		emus[i].cover = NULL;
		
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
				Video_WaitPanel (TEX_HGL, 0, "Searching...|(B) Stop", romname, emusCnt);
				t = time(NULL)+1;
				}

			strcpy (romname, fsop_GetFilename (emus[i].name, true));
			
			if (ms_strcmp (pent->d_name, romname))
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
	EmuBrowse (false);
	redraw = 1;
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
	
static int qsort_hidden (const void * a, const void * b)
	{
	s_emu *aa = (s_emu*) a;
	s_emu *bb = (s_emu*) b;

	if (aa->hidden > bb->hidden) return -1;
	if (aa->hidden < bb->hidden) return 1;
	return 0;
	}
	
static void SortItems (void)
	{
	int i;
	int filtered = 0;
	
	Debug ("SortItems: FixFilters");
	FixFilters	();
	
	Debug ("SortItems: Appling filters...");
	
	// Apply filters
	emus2Disp = 0;
	for (i = 0; i < emusCnt; i++)
		{
		emus[i].filtered = IsFiltered (i);
		if (emus[i].filtered && (!emus[i].hidden || showHidden)) emus2Disp++;
		if (emus[i].filtered) filtered++;
		}
	
	Video_PanelsUpdateTime (100);
	
	Video_WaitIcon (TEX_HGL);
	qsort (emus, emusCnt, sizeof(s_emu), qsort_filter);
	Video_WaitIcon (TEX_HGL);
	qsort (emus, filtered, sizeof(s_emu), qsort_hidden);
	Video_WaitIcon (TEX_HGL);
	qsort (emus, emus2Disp, sizeof(s_emu), qsort_name);
	
	Video_PanelsUpdateTime (0);

	pageMax = (emus2Disp-1) / gui.spotsXpage;
	
	Debug ("SortItems: FeedCoverCache...");
	FeedCoverCache ();
	
	Debug ("SortItems: done");
	}
	
// This will check if cover is available
static void CheckForCovers (void)
	{
	int i;
	
	// Cleanup cover...
	for (i = 0; i < emusCnt; i++)
		{
		if (emus[i].cover) free (emus[i].cover);
		emus[i].cover = NULL;
		}
		
	char *dir = fsop_GetDirAsString (vars.coversEmu, '|', 1, NULL);
	
	Debug ("fsop_GetDirAsString = '%s' = 0x%X", vars.coversEmu, dir);
	
	if (dir)
		{
		for (i = 0; i < emusCnt; i++)
			{
			char filename[128];
			
			strcpy (filename, fsop_GetFilename (emus[i].name, true));
			
			char *p = ms_strstr (dir, filename);
			if (p)
				{
				char *cover = ms_GetDelimitedString (p, '|', 0);
				
				if (ms_strcmp (fsop_GetFilename(cover, true), filename) == 0)
					{
					emus[i].cover = ms_AllocCopy (fsop_GetExtension(cover),0);
					free (cover);
					}
				else
					emus[i].cover = cover;
				}
			}

		free (dir);
		}
	}
	
static char *GetDolById (char *id)
	{
	int i;
	static char buff[64];
	
	*buff = '\0';
	for (i = 0; i < plugin.eumdolscount; i++)
		{
		if (!strcmp (id, plugin.emudols[i].id))
			{
			strcpy (buff, plugin.emudols[i].dol);
			break;
			}
		}
	return buff;
	}
	
static char *GetArgsById (char *id, int index)
	{
	int i;
	static char buff[64];
	
	Debug ("GetArgsById: %s,%d", id, index);
	
	*buff = '\0';
	for (i = 0; i < plugin.eumdolscount; i++)
		{
		char *argid = ms_GetDelimitedString  (id, ',', index);
		if (!argid) continue;
		
		if (!strcmp (argid, plugin.emudols[i].id))
			{
			free (argid);
			strcpy (buff, plugin.emudols[i].args);
			break;
			}
		free (argid);
		}
	return buff;
	}
	
static bool PluginExist (int index, char * dolpath, int idx) // idx -1 will check if any plugin exist
	{
	char *id;
	char *dol;
	char path[256];
	int j = 0;
	int i = 0;
	
	*dolpath = '\0';
	
	if (idx > -1) j = idx;
	
	do
		{
		id = ms_GetDelimitedString  (plugin.pin[index].id, ',', j++);
		if (!id) break;
		
		dol = GetDolById (id);
		free (id);

		if (!*dol) break;

		sprintf (path, "%s://ploader/plugins", vars.defMount);
		do
			{
			sprintf (dolpath, "%s/%s", path, dol);

			if (fsop_FileExist (dolpath)) return true;
				
			*path = '\0';
			char *buff = ms_GetDelimitedString (plugin.dolpath, '|', i++);
			if (buff)
				{
				strcpy (path, buff);
				free (buff);
				}
			}
		while (*path);
		
		if (idx > -1) break;
		}
	while (true);
	
	*dol = '\0';
	return false;
	}

static bool SearchPlugin (int index, char * dol, int num, int *order) // return the "num" active plugin
	{
	int i;
	int j = 0;
	
	for (i = 0; i < 16; i++)
		{
		if (PluginExist (index, dol, i))
			{
			if (num == j) 
				{
				*order = i;
				return true;
				}
			j++;
			}
		}
	
	*order = 0;
	return false;
	}
	
static int BrowsePluginFolder (int type, int startidx, char *path, int recursive)
	{
	int i = startidx;
	int rejected = 0;
	char fn[300];
	char ext[256];
	char item[256];
	char isFolder;
	char *dir, *p;
	static u64 t1 = 0;
	u64 t2;
	
	s_pin *pin = &plugin.pin[type];
	
	Debug ("BrowsePluginFolder (%d, %d, '%s', %d)", type, startidx, path, recursive);
	
	if (!pin->enabledCfg)
		{
		Debug ("# %s:%s => plugin not active", plugin.pin[type].description, path);
		return 0;
		}
	
	// Check if the one of the plugin exist (no check will be performed starting from multydol support
	/*
	if (!PluginExist (type, fn, -1))
		{
		Debug ("# %s:%s => plugin dol doesn't exist (%s)", plugin.pin[type].description, path, plugin.pin[type].dol);
		return 0;
		}
	*/
	strcpy (ext, pin->ext);

	t2 = gettime();
	if (diff_msec (t1, t2) > 200)
		{
		t1 = t2;
		Video_WaitPanel (TEX_HGL, 500, "Searching... %d roms found, %d%%|%s", i, (type * 100) / plugin.count, path);
		}

	//mt_Lock();
	p = dir = fsop_GetDirAsStringWithDirFlag (path, 0);
	//mt_Unlock();
	
	while (p && *p) 
		{
		strcpy (item, p);
		p += strlen(p) + 1;

		isFolder = (*p == '1') ? 1:0;
		p += 2;
		
		t2 = gettime();
		if (diff_msec (t1, t2) > 200)
			{
			t1 = t2;
			Video_WaitPanel (TEX_HGL, 500, "Searching... %d roms found, %d%%|%s", i, (type * 100) / plugin.count, path);
			}

		if (i >= EMUMAX)
			break;
		
		// Skip it
		if (strcmp (item, ".") == 0 || strcmp (item, "..") == 0)
			continue;

		if (isFolder && recursive)
			{
			char newpath[256];
			sprintf (newpath, "%s/%s", path, item);
			i += BrowsePluginFolder (type, i, newpath, 1);
			}
		else if (ms_strstr (ext, fsop_GetExtension(item)))
			{
			sprintf (fn, "%s/%s", path, item);
			if (plugin_AssignRomPath (fn, i)) 
				{
				usedBytes += (strlen (emus[i].name) + 1);
				emus[i].type = type;
				
				i++;
				}
			else
				rejected ++;
			}
		}
		
	if (dir) free (dir);
	
	Debug ("# %d roms found (%u kb), %d rejected (%s:%s)", i-startidx, usedBytes / 1024, rejected, plugin.pin[type].description, path);

	return i-startidx;
	}
	
static int EmuBrowse (bool rebuild)
	{
	Debug ("begin EmuBrowse");
	
	memset (&browseflag, 0, sizeof (s_browseflag));
	
	if (!plugin.count)
		{
		gui.spotsIdx = 0;
		gui_Clean ();
		StructFree ();
		Debug ("EmuBrowse: no plugins available");
		return 0;
		}

	if (rescanRoms || rebuild)
		{
		FILE *f;
		char cachefile[46];
		char path[300];
		char buff[300];
		int i, cnt;
		int dev;
		size_t size;
		
		sprintf (cachefile, "%s://ploader/emu.dat", vars.defMount);
		
		gui.spotsIdx = 0;
		gui_Clean ();
		StructFree ();
		
		Video_WaitIcon (TEX_HGL);
		
		Video_PanelsUpdateTime (100);
		
		f = fopen (cachefile, "rb");
		
		if (f && !rebuild)
			{
			Debug ("Emu Browse: loading from cache");
#define CACHESIZE 32768
#define CACHELIMIT CACHESIZE-256	// assume that 1 line is always less of this
			char *p;
			char *cache = calloc (CACHESIZE+1,1);
			int index = 0;
			int subsize = 0;
			do
				{
				Video_WaitIcon (TEX_HGL);
				
				// read first block
				if (subsize)
					memcpy (cache, &cache[index], subsize);

				size = fread (&cache[subsize], 1, CACHESIZE - subsize, f) + subsize;
				if (!size) break;

				cache[size] = '\0';
				
				index = 0;
				subsize = 0;
				
				p = cache;
				for (i = 0; i < size; i++)
					{
					if (*p == '\n') 
						{
						*p = 0;
						if (i >= CACHELIMIT-1)
							{
							index = i+1;
							subsize = CACHESIZE-index;
							break;
							}
						}
					p++;
					}
				
				if (index) size = index;
				
				p = cache;
				do
					{
					if (strstr (p, "{rp}"))
						{
						char * path = ms_GetDelimitedString (p, '|', 2);
						if (path)
							{
							plugin_AddRomPath (path);
							free (path);
							}
						}
					else
						{
						char *pp = strstr (p, "|"); if (pp) pp++;
						emus[emusCnt].type = (u8)atoi (p);
						emus[emusCnt].pathid = (s16)atoi (pp);
						emus[emusCnt].name = ms_GetDelimitedString (p, '|', 2);
						emus[emusCnt].cover = ms_GetDelimitedString (p, '|', 3);
						
						//Debug ("%d %u '%s' '%s'", emusCnt, emus[emusCnt].type, emus[emusCnt].name, emus[emusCnt].cover);
						
						emusCnt++;
						}
						
					p += strlen(p)+1;
					}
				while (*p && (p-cache) < size);
				
				//break;
				}
			while (true);
			
			fclose (f);
			free (cache);
			
			Video_PanelsUpdateTime (0);
			
			if (plugin.rompathscount == 0)
				{
				grlib_menu (50, "Error:\nThe file emu.dat seems to be old\nYou need to refresh the cache...", ":-(");
				StructFree ();
				}
			}
		else
			{
			Debug ("Emu Browse: searching for plugins data roms");
			
			plugin_FreeRompaths ();
			
			usedBytes = EMUMAX * sizeof(s_emu);
			emusCnt = 0;
			for (dev = 0; dev < DEV_MAX; dev++)
				{
				if (devices_Get (dev))
					{
					Debug ("Checking: %s (%d)", devices_Get (dev), plugin.count);
					
					for (i = 0; i < plugin.count; i++)
						{
						s_pin *pin = &plugin.pin[i];

						sprintf (path, "%s:/%s", devices_Get (dev), pin->romfolder);
						cnt = BrowsePluginFolder (i, emusCnt, path, pin->recurse);
						emusCnt += cnt;
						//Debug ("%s:found %d roms in %s", pin->description, cnt, path);
						}
					}
				}
					
			scanned = 1;
			
			Debug ("Allocated %d bytes (%d Kb)", usedBytes, usedBytes / 1024);
			
			for (i = 0; i < plugin.rompathscount; i++)
				{
				Debug ("RP> %d:%s", i, plugin.rompaths[i]);
				}
			
			CheckForCovers ();

			// So, let's write the cache file
			f = fopen (cachefile, "wb");
			if (f)
				{
				// writing folders....
				for (i = 0; i < plugin.rompathscount; i++)
					{
					sprintf (buff, "%u|{rp}|%s\n", i, plugin.rompaths[i]);
					fwrite (buff, 1, strlen(buff), f);
					}
				for (i = 0; i < emusCnt; i++)
					{
					sprintf (buff, "%u|%d|%s|%s\n", emus[i].type, emus[i].pathid, emus[i].name, emus[i].cover ? emus[i].cover: "");
					fwrite (buff, 1, strlen(buff), f);
					}
				fclose (f);
				}
			else
				{
				grlib_menu (50, "Cannot write cache file", ":(");
				}
			
			
			rescanRoms = 0;
			}

		// Load roms config
		/*
		for (i = 0; i < emusCnt; i++)
			ReadGameConfig (i);
		*/
		// Let's clean undeeded plugins

		for (i = 0; i < plugin.count; i++)
			{
			plugin.pin[i].count = CountRomsPlugin(i);
			Debug ("> %s (enabled=%d, count=%d)", plugin.pin[i].description, plugin.pin[i].enabled, plugin.pin[i].count);

			if (!plugin.pin[i].count)
				plugin.pin[i].enabled = 0;
			}
		}

	SortItems ();

	Debug ("end EmuBrowse");
	
	redraw = 1;

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
			sprintf (buff, "%s: %s", plugin.pin[emus[emuSelected].type].description, fsop_GetFilename (emus[emuSelected].name, true));

			char title[256];
			strcpy (title, buff);
			if (strlen (title) > 48)
				{
				title[48] = 0;
				strcat (title, "...");
				}
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, title);

			Video_SetFont(TTFSMALL);
			grlib_printf (XMIDLEINFO, theme.line1Y, GRLIB_ALIGNCENTER, 0, fsop_GetPath(emus[emuSelected].name, 0));
			
			t = time(NULL);
			break;
			}
		}
	
	Video_SetFont(TTFSMALL);
	if (!grlib_irPos.valid)
		{
		if (emuSelected == -1) grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "Point at an icon with the wiimote or use a GC controller!");
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

	int j = 0;
	for (i = 0; i < plugin.count; i++)
		if (GBFA (i, config.emuFilter)) j++;

	if (j == 0x0 || j == plugin.count)
		{
		return true;
		}
	
	for (i = 0; i < EMUPINMAX; i++)
		{
		if (GBFA (i, config.emuFilter) && emus[ai].type == i)
			{
			ret = true;
			}
		}

	return ret;
	}
	
static void FixFilters (void)
	{
	int i;
	u8 f[EMUPINMAX];
	
	for (i = 0; i < plugin.count; i++)
		{
		f[i] = (GBFA (i, config.emuFilter) && CountRomsPlugin(i));
		}

	for (i = 0; i < plugin.count; i++)
		SBIA (i, f[i], config.emuFilter);
	}
	
static void ShowFilterMenu (void)
	{
	char buff[2048];
	char title[128];
	int active;
	int lut[EMUPINMAX];
	u8 f[EMUPINMAX];
	int i, item, lines, addCol = 0, page = 0, pages, buttons;
	
	active = 0;
	for (i = 0; i < plugin.count; i++)
		{
		f[i] = GBFA (i, config.emuFilter);
		if (plugin.pin[i].enabled)
			{
			lut[active++] = i;
			}
		}

	for (i = 0; i < plugin.count; i++)
		f[i] = GBFA (i, config.emuFilter);

	lines = 8;
	buttons = lines * 3; // there are 3 columns
	pages = active / buttons;
		
	do
		{
		*buff = '\0';
		int col = 0;
		int assigned = 0;
		for (i = page * buttons; i < (page+1) * buttons; i++)
			{
			if (i < active)
				{
				if (col == (lines) || col == (lines*2)) addCol = 1;
				if (addCol)
					{
					grlib_menuAddColumn (buff);
					addCol = 0;
					}

				grlib_menuAddCheckItem (buff, 100 + i, f[lut[i]], "%s (%u)", plugin.pin[lut[i]].description, plugin.pin[lut[i]].count);

				col ++;
				assigned ++;
				}
			}
			
		for (i = 0; i < buttons - assigned; i++)
			{
			if (col == (lines) || col == (lines*2)) addCol = 1;
			if (addCol)
				{
				grlib_menuAddColumn (buff);
				addCol = 0;
				}
			grlib_menuAddItem (buff, 100 + i, "_");
			col ++;
			}
		
		Video_SetFontMenu(TTFVERYSMALL);
		if (pages == 0)
			sprintf (title, "Filter menu\n(B) close, (+) Select all, (-) Deselect all");
		else
			sprintf (title, "Filter menu :: Page %d of %d\n(B) close, (+) Select all, (-) Deselect all, (<) Previous page, (>) Next page", page+1, pages+1);
		item = grlib_menu (150, title, buff);
		Video_SetFontMenu(TTFNORM);

		if (item == MNUBTN_RIGHT)
			{
			page++;
			if (page > pages) page = 0;
			}

		if (item == MNUBTN_LEFT)
			{
			page--;
			if (page < 0) page = pages;
			}

		if (item == MNUBTN_PLUS)
			{
			int i; 	for (i = 0; i < plugin.count; i++) f[i] = 1;
			}

		if (item == MNUBTN_MINUS)
			{
			int i; 	for (i = 0; i < plugin.count; i++) f[i] = 0;
			}
		
		if (item >= 100)
			{
			int i = item - 100;
			f[lut[i]] = !f[lut[i]];
			}
		}
	while (item != -1);
	
	for (i = 0; i < plugin.count; i++)
		SBIA (i, f[i], config.emuFilter);
	
	EmuBrowse (false);
	}

static void ShowAppMenu (int ai)
	{
	char buff[512];
	char title[256];
	
	if (!CheckParental(0)) return;
	
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

		//grlib_menuAddItem (buff,  1, "Played %d times", emus[ai].playcount);
		
		//if (CheckParental(0))
		//	{
			grlib_menuAddSeparator (buff);
			grlib_menuAddItem (buff,  2, "Delete this rom (and cover)");
			grlib_menuAddItem (buff,  3, "Delete this cover");
		//	}
		
		int item = grlib_menu (0, title, buff);
		
		if (item < 0) break;
		
		if (item == 1)
			{
			int item = grlib_menu (-60, "Reset play count ?", "Yes~No");
			
			if (item == 0)
				{
				emus[ai].playcount = 0;
				SortItems ();
				}
			}
			
		if (item == 2)
			{
			sprintf (buff, "Are you sure you want to delete this ROM and it's cover ?\n\n%s", title);
			int item = grlib_menu (-60, buff, "Yes~No");
			
			if (item == 0)
				{
				unlink (plugin_GetRomFullPath (ai));
				
				char cover[256];
				MakeCoverPath (ai, cover);
				
				unlink (cover);
				
				EmuBrowse (true);
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

				EmuBrowse (false);
				break;
				}
			}
		}
	while (TRUE);
	
	WriteGameConfig (ai);

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
	
	Debug ("CheckParental = %d", CheckParental(0));

	if (CheckParental(0))
		{
		/*
		if (showHidden)
			grlib_menuAddItem (buff,  6, "Hide hidden emus");
		else
			grlib_menuAddItem (buff,  7, "Show hidden emus");
		*/	
		grlib_menuAddItem (buff,  1, "Import snapshots as covers");
		}
		
	grlib_menuAddItem (buff,  2, "Auto import retroarch snapshots (%s)", config.enableRetroarchAutocover ? "Yes" : "No");

	grlib_menuAddItem (buff,  4, "Refresh cache");

	if (config.lockCurrentMode)
		grlib_menuAddItem (buff,  20, "Unlock this mode");
	else
		grlib_menuAddItem (buff,  20, "Lock this mode");

	Redraw();
	grlib_PushScreen();
	
	int item = grlib_menu (0, "Emulators menu", buff);
	
	if (item == 20)
		{
		config.lockCurrentMode = !config.lockCurrentMode;
		}
		
	if (item == 1)
		{
		GetCovers ();
		goto start;
		}

	if (item == 2)
		{
		config.enableRetroarchAutocover = 1 - config.enableRetroarchAutocover;
		goto start;
		}

	if (item == 4)
		{
		EmuBrowse (true);
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

static char *GetFilterString (int maxwidth)
	{
	static u8 lastfilter[32];
	static char string[1024] = {0};
	int i;
	int filterd = 0, active = 0;
	int emusCntLast = 0;

	for (i = 0; i < plugin.count; i++)
		{
		if (GBFA (i, config.emuFilter)) filterd++;
		if (plugin.pin[i].enabled) active++;
		}
	
	if (memcmp (lastfilter, config.emuFilter, sizeof (config.emuFilter)) == 0 && strlen(string) && emusCntLast == emusCnt) // nothing changed
		return string;
		
	emusCntLast = emusCnt;
		
	memcpy (lastfilter, config.emuFilter, sizeof (config.emuFilter));
	
	sprintf (string, "(%d ROMs) ", emus2Disp);
	
	if (filterd == 0 ||  filterd == active)
		{
		return string;
		}
	
	for (i = 0; i < plugin.count; i++)
		if (GBFA (i, config.emuFilter))
			{
			strcat (string, plugin.pin[i].description);
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
	
static void Redraw (void)
	{
	Video_DrawBackgroud (1);
	
	Video_SetFont(TTFNORM);

	int w1 = grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader::Emulators");
	int w2 = grlib_printf ( 615, 26, GRLIB_ALIGNRIGHT, 0, "Page %d of %d", page+1, pageMax+1);
	
	w1 = w1 + 25;
	w2 = 615 - w2;
	
	Video_SetFont(TTFVERYSMALL);
	grlib_printf ( w1 + (w2 - w1) / 2, 27, GRLIB_ALIGNCENTER, 0, GetFilterString((w2 - w1) - 30));
	
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
	bool ret = false;
	int i;
	float mag = 1.0;
	float magMax = 3.1;
	int spot = -1;
	int incX = 1, incY = 1;
	int y = 200;
	int yInf = 500, yTop = 420;
	char dols[16][128];
	int dolsNum = 0;
	char buff[128];
	u32 transp = 32, transpLimit = 224;
	
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
	istemp.transparency = 255;
	
	s_grlibobj black;
	black.x1 = 0;
	black.y1 = 0;
	black.x2 = grlib_GetScreenW();
	black.y2 = grlib_GetScreenH();
	
	// Load full res cover
	char path[300];
	MakeCoverPath (ai, path);
	//mt_Lock();
	GRRLIB_texImg * tex = GRRLIB_LoadTextureFromFile (path);
	//mt_Unlock();
	if (tex) ico.icon = tex;
	
	Video_SetFont(TTFNORM);
	
	for (i = 0; i < 16; i++)
		{
		if (PluginExist(emus[emuSelected].type, buff, i))
			{
			strcpy (dols[dolsNum], fsop_GetFilename (buff, true));
			dolsNum++;
			}
		}
	
	if (config.emuDols[emus[emuSelected].type] >= dolsNum) 
		config.emuDols[emus[emuSelected].type] = 0;
		
	if (dolsNum > 1)
		sprintf (buff, "(<) %s (>)", dols[config.emuDols[emus[emuSelected].type]]);
	else if (dolsNum == 1)
		sprintf (buff, "%s", dols[config.emuDols[emus[emuSelected].type]]);
	else
		sprintf (buff, "!!! no dol available !!!");
	
	while (true)
		{
		grlib_PopScreen ();
		
		black.color = RGBA(0,0,0,transp);
		black.bcolor = RGBA(0,0,0,transp);
		grlib_DrawSquare (&black);
		transp += 4;
		if (transp > transpLimit) transp = transpLimit;

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

		DrawInfoWindow (yInf, fsop_GetFilename(emus[emuSelected].name, true), "Press (A) to start, (B) Cancel", buff);
		yInf -= 5;
		if (yInf < yTop) yInf = yTop;

		Overlay ();
		grlib_DrawIRCursor ();
		grlib_Render();
		
		if (mag < magMax) mag += 0.1;
		if (mag >= magMax && ico.x == 320 && ico.y == y) break;
		}
	
	u32 btn;
	while (true)
		{
		grlib_PopScreen ();

		black.color = RGBA(0,0,0,transp);
		black.bcolor = RGBA(0,0,0,transp);
		grlib_DrawSquare (&black);
		transp += 4;
		if (transp > transpLimit) transp = transpLimit;

		grlib_IconDraw (&istemp, &ico);
		Overlay ();
		
		DrawInfoWindow (yInf, fsop_GetFilename(emus[emuSelected].name, true), "Press (A) to start, (B) Cancel", buff);
		yInf -= 5;
		if (yInf < yTop) yInf = yTop;
		
		grlib_DrawIRCursor ();
		grlib_Render();

		btn = grlib_GetUserInput();
		if (btn & WPAD_BUTTON_A)
			{
			ret = true;
			break;
			}
		if (btn & WPAD_BUTTON_B)
			{
			ret = false;
			break;
			}
		if (btn & WPAD_BUTTON_RIGHT && dolsNum > 1)
			{
			config.emuDols[emus[emuSelected].type]++;
			
			if (config.emuDols[emus[emuSelected].type] >= dolsNum) config.emuDols[emus[emuSelected].type] = 0;
			if (dolsNum) sprintf (buff, "(<) %s (>)", dols[config.emuDols[emus[emuSelected].type]]);
			}
		if (btn & WPAD_BUTTON_LEFT && dolsNum > 1)
			{
			config.emuDols[emus[emuSelected].type]--;
			
			if (config.emuDols[emus[emuSelected].type] == 0xFF) config.emuDols[emus[emuSelected].type] = dolsNum-1;
			if (dolsNum) sprintf (buff, "(<) %s (>)", dols[config.emuDols[emus[emuSelected].type]]);
			}
		}
		
	while (ico.transparency > 10)
		{
		istemp.transparency = ico.transparency;
		grlib_PopScreen ();

		black.color = RGBA(0,0,0,transp);
		black.bcolor = RGBA(0,0,0,transp);
		grlib_DrawSquare (&black);
		if (transp) transp -= 4;

		grlib_IconDraw (&istemp, &ico);
		ico.transparency -= 10;
		Overlay ();
		
		DrawInfoWindow (yInf, fsop_GetFilename(emus[emuSelected].name, true), "Press (A) to start, (B) Cancel", buff);
		yInf += 5;

		grlib_DrawIRCursor ();
		grlib_Render();
		}

	GRRLIB_FreeTexture (tex);
	return ret;
	}

static void StartEmu (int type, char *fullpath)
	{
	char cmd[256] = {0};
	char dol[256];
	int order;

	if (SearchPlugin(type, dol, config.emuDols[type], &order))	// theorically this check isn't needed...
		{
		//if (order > -1) order = 0;
		
		char *p = GetArgsById (plugin.pin[type].id, order);
		if (p) 
			{
			strcpy (cmd, p);
			}
			
		if (*cmd == 0)
			{
			grlib_menu (0, "Attention!", "plugins.conf seems to be invalid or corrupted");
			return;
			}
			
		Debug ("StartEmu: cmd='%s'", cmd);
			
		char loader[256];
		sprintf (loader, "%s://ploader/plugins/forwarder.dol", vars.defMount);
		
		Debug ("fsop_GetDev: '%s'", fsop_GetDev(fullpath));
		Debug ("fsop_GetPath: '%s'", fsop_GetPath(fullpath, 1));
		Debug ("fsop_GetFilename: '%s'", fsop_GetFilename(fullpath, 0));

		ms_Subst (cmd, "{device}", fsop_GetDev (fullpath));
		ms_Subst (cmd, "{path}", fsop_GetPath (fullpath, 1));
		ms_Subst (cmd, "{name}", fsop_GetFilename (fullpath, 0));
		ms_Subst (cmd, "{loader}", loader);
		ms_Subst (cmd, "{titlelow}", "00010001");
		ms_Subst (cmd, "{titlehi}", "504F5354");
		ms_Subst (cmd, "{loadername}", "postLoader");
		ms_Subst (cmd, "$", ";");
		ms_Subst (cmd, DEV_MN_USB1, "usb1:");
		ms_Subst (cmd, DEV_MN_USB2, "usb2:");
		ms_Subst (cmd, DEV_MN_USB3, "usb3:");

		Debug ("StartEmu: cmd='%s'", cmd);
		
		Conf (false);	// Store configuration on disc
		Plugins (false);

		strcpy (config.lastRom, fsop_GetFilename (fullpath, 0));
		DirectDolBoot (dol, cmd, 0);
		}

	grlib_menu (0, "Attention!", "Requested plugin not found");
	}
			
int EmuBrowser (void)
	{
	u32 btn;

	Debug ("GameBrowser (begin)");
	
	grlibSettings.color_window = RGBA(192,192,192,255);
	grlibSettings.color_windowBg = RGBA(32,32,32,128);
	
	grlib_SetRedrawCallback (Redraw, Overlay);
	
	Video_WaitIcon (TEX_HGL);	
	
	plugin_Init ();

	Plugins (true);
	Conf (true);
	
	scanned = 0;
	browserRet = -1;
	
	usedBytes = EMUMAX * sizeof(s_emu);
	emus = (s_emu*) vars.bigblock; //malloc (usedBytes);
	memset (emus, 0, usedBytes);
	
	Debug ("GameBrowser: Allocated %d Kb for emu structure:", usedBytes);
	
	// Immediately draw the screen...
	StructFree ();
	InitializeGui ();
	
	/*
	Redraw ();
	grlib_PushScreen ();
	grlib_PopScreen ();
	grlib_Render();  // Render the theme.frame buffer to the TV
	*/

	config.enableRetroarchAutocover = 1;
	retroarch_GetCover ();
	
	page = config.gamePageEmu;
	rescanRoms = 1;
	EmuBrowse (false);
	
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
				
				StartEmu (emus[emuSelected].type, plugin_GetRomFullPath (emuSelected));

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
			EmuBrowse (false);
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
	//free (emus);
	
	grlib_SetRedrawCallback (NULL, NULL);
	
	return browserRet;
	}