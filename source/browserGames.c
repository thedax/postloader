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


#define GAMEMAX 1024

/*

This allow to browse for applications in games folder of mounted drive

*/
extern s_grlibSettings grlibSettings;
static s_gameConfig gameConf;

static int browse = 0;
static int scanned = 0;

#define CATN 32
#define CATMAX 32

static char *flt[CATN] = { 
"Action",
"Adventure",
"Sport",
"Racing",
"Rhythm",
"Simulation",
"Platformer",
"Party",

"Music",
"Puzzle",
"Fighting",
"Shooter",
"Strategy",
"RPG",
"FPS",
"TPS",

"Online",
"DS Connect",
"ESRB M 3+",
"ESRB E 6+",
"ESRB E 10+",
"ESRB T 13+",
"ESRB M 17+",
"ESRB A 18+",

"uDraw", 		
"Bal. board",    
"Instrument", 	
"Zapper", 		
"Microphone", 	
"Motion+",		
"Wheel",
"Dance Pad"
};

static s_game *games;
static int gamesCnt;
static int games2Disp;
static int page; // Page to be displayed
static int pageMax; // 
static int gamesSelected = -1;	// Current selected app with wimote

static int browserRet = 0;
static int showHidden = 0;

static u8 redraw = 1;
static bool redrawIcons = true;

static s_grlib_iconSetting is;

static void Redraw (void);
static int GameBrowse (int forcescan);

char* neek_GetGames (void);
void neek_KillDIConfig (void);
static bool IsFiltered (int ai);

static int disableSpots;

static s_cfg *cfg;

#define ICONW 80
#define ICONH 150

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
	gui.spotsXpage = 12;
	gui.spotsXline = 6;

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
	is.borderSelColor = RGBA (255,255,255,255);
	is.borderColor = RGBA (128,128,128,255);
	is.fontReverse = 0;
	
	Debug ("theme.frameBack = 0x%X 0x%X", theme.frameBack, is.bkgTex);

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

static void MakeCoverPath (int ai, char *path)
	{
	char asciiId[10];
	
	strcpy (asciiId, games[ai].asciiId);

	if (config.gameMode == GM_DML)
		asciiId[6] = 0;

	sprintf (path, "%s/%s.png", vars.covers, asciiId);
	}

static void FeedCoverCache (void)
	{
	char path[128];
	CoverCache_Pause (true);

	if (page > pageMax) page = pageMax;
	if (page < 0) page = 0;

	int i;
	int ai;	// Application index (corrected by the offset)

	for (i = -gui.spotsXpage; i < gui.spotsXpage*2; i++)
		{
		ai = (page * gui.spotsXpage) + i;
		
		if (ai >= 0 && ai < gamesCnt && games[ai].hasCover)
			{
			MakeCoverPath (ai, path);
			CoverCache_Add (path, (i >= 0 && i < gui.spotsXpage) ? true:false );
			}
		}
	
	CoverCache_Pause (false);
	}

static bool DownloadCovers_Get (char *path, char *buff)
	{
	u8* outbuf=NULL;
	u32 outlen=0;
	
	Debug ("DownloadCovers_Get: calling downloadfile");
	outbuf = downloadfile (buff, &outlen, NULL);
		
	Debug ("DownloadCovers_Get: checking png");
	if (IsPngBuff (outbuf, outlen))
		{
		//suficientes bytes
		FILE *f;
		
		f = mt_fopen (path, "wb");
		if (f)
			{
			Debug ("DownloadCovers_Get: writing png");
			mt_fwrite (outbuf, outlen, 1, f);
			mt_fclose (f);
			}
		
		free(outbuf);
		return (TRUE);
		}
		
	return (FALSE);
	}

static void DownloadCovers (void)
	{
	int ia, stop;
	char buff[300];
	char path[PATHMAX];

	Redraw ();
	grlib_PushScreen ();
	
	Video_WaitPanel (TEX_HGL, "Pausing wiiload thread");
	WiiLoad_Pause ();
	
	stop = 0;

	FILE *f = NULL;
	if (devices_Get(DEV_SD))
		{
		Debug ("DownloadCovers: opening sd:/missgames.txt");
		sprintf (path, "%s://missgame.txt", devices_Get(DEV_SD));
		f = mt_fopen (path, "wb");
		}
	
	for (ia = 0; ia < gamesCnt; ia++)
		{
		Video_WaitPanel (TEX_HGL, "Downloading %s.png (%d of %d)|(B) Stop", games[ia].asciiId, ia, gamesCnt);
		sprintf (path, "%s/%s.png", vars.covers, games[ia].asciiId);
		
		mt_Lock();
		int ret = fsop_FileExist(path);
		mt_Unlock();

		if (!ret)
			{
			bool ret = FALSE;
			
			if (!ret)
				{
				sprintf (buff, "http://art.gametdb.com/wii/cover/US/%s.png", games[ia].asciiId);
				ret = DownloadCovers_Get (path, buff);
				}
			if (!ret)
				{
				sprintf (buff, "http://art.gametdb.com/wii/cover/EN/%s.png", games[ia].asciiId);
				ret = DownloadCovers_Get (path, buff);
				}
			if (!ret)
				{
				sprintf (buff, "http://art.gametdb.com/wii/cover/JA/%s.png", games[ia].asciiId);
				ret = DownloadCovers_Get (path, buff);
				}
			if (!ret)
				{
				sprintf (buff, "http://www.postloader.freehosting.com/png/%s.png", games[ia].asciiId);
				ret = DownloadCovers_Get (path, buff);
				}
				
			if (!ret && f)
				{
				sprintf (buff, "%s:%s\n", games[ia].asciiId, games[ia].name);
				mt_fwrite (buff, 1, strlen(buff), f);
				}
				
			if (grlib_GetUserInput () == WPAD_BUTTON_B)
				{
				stop = 1;
				break;
				}
			}
					
		if (stop) break;
		}
		
	if (f) mt_fclose (f);
		
	WiiLoad_Resume ();
	
	CoverCache_Flush ();	
	GameBrowse (0);
	}

static void WriteGameConfig (int ia)
	{
	if (ia < 0) return;

	strcpy (gameConf.name, games[ia].name);
	strcpy (gameConf.asciiId, games[ia].asciiId);
	gameConf.hidden = games[ia].hidden;
	gameConf.priority = games[ia].priority;
	gameConf.category = games[ia].category;
	gameConf.playcount = games[ia].playcount;
	
	char buff[2048];
	int index = 0;
	*buff = '\0';
	
	cfg_Section (NULL);
	cfg_FmtString (buff, CFG_WRITE, CFG_STRING,		gameConf.name, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_INT, 		&gameConf.priority, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.hidden, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.ios, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.vmode, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_S8, 		&gameConf.language, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_S8, 		&gameConf.vpatch, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.hook, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.ocarina, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.nand, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.loader, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U16, 		&gameConf.playcount, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.category, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.minPlayerAge, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.dmlVideoMode, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.dmlNoDisc, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.dmlFullNoDisc, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.dmlPadHook, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.dmlNMM, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.memcardId, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.widescreen, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.wifi, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8, 		&gameConf.activity_led, index++);

	cfg_SetString (cfg, games[ia].asciiId, buff);
	}

static int ReadGameConfig (int ia)
	{
	char buff[2048];
	bool valid;

	valid = (cfg_GetString (cfg, games[ia].asciiId, buff) != -1 && cfg_CountSepString (buff) >= 23);
	
	if (valid)
		{
		int index = 0;
		cfg_FmtString (buff, CFG_READ, CFG_STRING,	gameConf.name, index++);
		cfg_FmtString (buff, CFG_READ, CFG_INT, 	&gameConf.priority, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.hidden, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.ios, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.vmode, index++);
		cfg_FmtString (buff, CFG_READ, CFG_S8, 		&gameConf.language, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.hook, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.ocarina, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.nand, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.loader, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U16, 	&gameConf.playcount, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.category, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.minPlayerAge, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.dmlVideoMode, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.dmlNoDisc, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.dmlFullNoDisc, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.dmlPadHook, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.dmlNMM, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.memcardId, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.widescreen, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.wifi, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8, 		&gameConf.activity_led, index++);
		}
	
	if (!valid)
		{
		strcpy (gameConf.name, games[ia].name);
		
		gameConf.priority = 5;
		gameConf.hidden = 0;
		gameConf.playcount = 0;
		gameConf.category = 0;
		
		gameConf.ios = 0; 		// use current
		gameConf.vmode = 0;
		gameConf.language = -1;
		gameConf.vpatch = 0;
		gameConf.ocarina = 0;
		gameConf.hook = 0;
		gameConf.loader = config.gameDefaultLoader;

		/*CONF_GetRegion() == CONF_REGION_EU*/ 
		if (games[ia].asciiId[3] == 'E' || games[ia].asciiId[3] == 'J' || games[ia].asciiId[3] == 'N')
			gameConf.dmlVideoMode = DMLVIDEOMODE_NTSC;
		else
			gameConf.dmlVideoMode = DMLVIDEOMODE_PAL;
		}

	if (strcmp (games[ia].name, gameConf.name) && *gameConf.name != '\0')
		{
		if (games[ia].name) free (games[ia].name);
		games[ia].name = ms_AllocCopy (gameConf.name, 0);
		if (!games[ia].name) 
			{
			games[ia].name = ms_AllocCopy (games[ia].asciiId, 0);
			}
		}

	games[ia].category = gameConf.category;
	games[ia].hidden = gameConf.hidden;
	games[ia].priority = gameConf.priority;
	games[ia].playcount = gameConf.playcount;
	
	return valid;
	}

static void StructFree (void)
	{
	int i;
	
	for (i = 0; i < gamesCnt; i++)
		{
		games[i].png = NULL;
		
		if (games[i].name != NULL) 
			{
			free (games[i].name);
			games[i].name = NULL;
			}
		}
		
	gamesCnt = 0;
	}
	
static int qsort_name (const void * a, const void * b)
	{
	s_game *aa = (s_game*) a;
	s_game *bb = (s_game*) b;
	
	if (!aa->name || !bb->name) return 0;
	
	return (ms_strcmp (aa->name, bb->name));
	}

static int bsort_filtered (const void * a, const void * b)
	{
	s_game *aa = (s_game*) a;
	s_game *bb = (s_game*) b;
	
	if (aa->filtered < bb->filtered) return 1;
	
	return 0;
	}

static int bsort_hidden (const void * a, const void * b)
	{
	s_game *aa = (s_game*) a;
	s_game *bb = (s_game*) b;
	
	if (aa->hidden > bb->hidden) return 1;

	return 0;
	}

static int bsort_priority (const void * a, const void * b)
	{
	s_game *aa = (s_game*) a;
	s_game *bb = (s_game*) b;

	if (aa->priority < bb->priority) return 1;
	
	return 0;
	}

static int bsort_playcount (const void * a, const void * b)
	{
	s_game *aa = (s_game*) a;
	s_game *bb = (s_game*) b;

	if (aa->playcount < bb->playcount) return 1;
	
	return 0;
	}

static int bsort_slot (const void * a, const void * b)
	{
	s_game *aa = (s_game*) a;
	s_game *bb = (s_game*) b;
	
	if (aa->slot > bb->slot) return 1;
	
	return 0;
	}

static void SortItems (void)
	{
	int i;
	int filtered = 0;

	// Apply filters
	games2Disp = 0;
	for (i = 0; i < gamesCnt; i++)
		{
		games[i].filtered = IsFiltered (i);
		if (games[i].filtered && (!games[i].hidden || showHidden)) games2Disp++;
		if (games[i].filtered) filtered++;
		}
	
	bsort (games, gamesCnt, sizeof(s_game), bsort_filtered);
	bsort (games, filtered, sizeof(s_game), bsort_hidden);
	qsort (games, games2Disp, sizeof(s_game), qsort_name);

	if (config.gameSort == 0)
		bsort (games, games2Disp, sizeof(s_game), bsort_priority);

	if (config.gameSort == 2)
		bsort (games, games2Disp, sizeof(s_game), bsort_playcount);

	if (config.gameMode == GM_DML)
		bsort (games, games2Disp, sizeof(s_game), bsort_slot);

	pageMax = (games2Disp-1) / gui.spotsXpage;
	
	FeedCoverCache ();
	}
	
static void UpdateTitlesFromTxt (void)
	{
	Debug ("UpdateTitlesFromTxt: begin");
	LoadTitlesTxt ();
	if (vars.titlestxt == NULL)
		{
		Debug ("Invalid titlestxt");
		return;
		}

	int i;
	char *title;
	for (i = 0; i < gamesCnt; i++)
		{
		title = cfg_FindInBuffer (vars.titlestxt, games[i].asciiId);
		if (title)
			{
			//Debug ("%s %s -> '%s'", games[i].asciiId, games[i].name, title);
			free (games[i].name);
			games[i].name = ms_utf8_to_ascii (title);
			
			WriteGameConfig (i);
			}
		
		if (i % 20 == 0) Video_WaitIcon (TEX_HGL);
		}
		
	free (vars.titlestxt);
	vars.titlestxt = NULL;
	Debug ("UpdateTitlesFromTxt: end");
	}
	
// This will check if cover is available
static void CheckForCovers (void)
	{
	DIR *pdir;
	struct dirent *pent;

	char path[256];
	int i;
	
	// Cleanup cover flag
	for (i = 0; i < gamesCnt; i++)
		games[i].hasCover = 0;
		
	sprintf (path, "%s", vars.covers);
	
	mt_Lock();
	pdir=opendir(path);
	mt_Unlock();
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0 || ms_strstr (pent->d_name, ".png") == NULL)
			continue;
			
		for (i = 0; i < gamesCnt; i++)
			{
			if (!games[i].hasCover && ms_strstr (pent->d_name, games[i].asciiId))
				games[i].hasCover = 1;
			}
		}
	closedir(pdir);
	}


static int GameBrowse (int forcescan)
	{
	char slot[8];
	Debug ("begin GameBrowse");
	
	gui.spotsIdx = 0;
	gui_Clean ();
	StructFree ();

	int i;
	char *titles = NULL;
	char *p;
	
	Video_WaitIcon (TEX_HGL);
	
	mt_Lock();

#ifndef DOLPHINE	
	if (config.gameMode == GM_WII)
		{
		if (vars.neek != NEEK_NONE) // use neek interface to build up game listing
			titles = neek_GetGames ();
		else
			titles = WBFSSCanner (forcescan);
		}
	else
		{
		titles = DMLScanner (forcescan);
		}
#else
	if (config.gameMode == GM_WII)
		{
		titles = WBFSSCanner (forcescan);
		}
	else
		{
		titles = DMLScanner (forcescan);
		}
#endif
	
	mt_Unlock();
	
	if (!titles) 
		{
		games2Disp = 0;
		gamesCnt = 0;
		page = 0;
		pageMax = 0;
		return 0;
		}
	
	p = titles;
	i = 0;
	
	Debug ("GameBrowse: parsing title cache buffer");
			
	do
		{
		if (*p != '\0' && strlen(p))
			{
			// Add name
			//Debug ("name = %s (%d)", p, strlen(p));
			games[i].name = malloc (strlen(p)+1);
			strcpy (games[i].name, p);
			p += (strlen(p) + 1);
			
			// Add id
			//Debug ("id = %s (%d)", p, strlen(p));
			strncpy (games[i].asciiId, p, 6);
			if (config.gameMode == GM_DML)
				games[i].disc = p[6];
			p += (strlen(p) + 1);
				
			if (config.gameMode == GM_WII)
				{
				// Setup slot
				strcpy (slot, p);
				p += (strlen(p) + 1);

				games[i].slot = atoi (slot); // PArtition number
				}
			else
				{
				strcpy (slot, p);
				p += (strlen(p) + 1);

				games[i].slot = atoi (slot); // sd = 0 / usb = 1...3?

				strcpy (games[i].source, p);
				p += (strlen(p) + 1);
				}
			
			Debug (" > %s (%s:%d:%d) in '%s'", games[i].name, games[i].asciiId, games[i].disc, games[i].slot, games[i].source);

			if (i % 20 == 0) Video_WaitIcon (TEX_HGL);
			
			if (!(config.gameMode == GM_DML && config.dmlVersion == GCMODE_DM22 && games[i].slot == 0))
				{
				ReadGameConfig (i);

				i ++;
				}
			}
		else
			break;
		}
	while (TRUE);
	
	gamesCnt = i;
	
	free (titles);

	scanned = 1;
	
	CheckForCovers ();

	Debug ("end GameBrowse");

	if (forcescan) UpdateTitlesFromTxt ();
	
	SortItems ();

	return gamesCnt;
	}

static int FindSpot (void)
	{
	int i;
	static time_t t = 0;
	char info[300];
	
	Video_SetFont(TTFNORM);
	
	gamesSelected = -1;
	
	if (disableSpots) return gamesSelected;
	
	for (i = 0; i < gui.spotsIdx; i++)
		{
		if (grlib_irPos.x > gui.spots[i].ico.rx1 && grlib_irPos.x < gui.spots[i].ico.rx2 && grlib_irPos.y > gui.spots[i].ico.ry1 && grlib_irPos.y < gui.spots[i].ico.ry2)
			{
			// Ok, we have the point
			gamesSelected = gui.spots[i].id;

			gui.spots[i].ico.sel = true;
			grlib_IconDraw (&is, &gui.spots[i].ico);

			strcpy (info, games[gamesSelected].name);
			
			if (vars.neek == NEEK_NONE) // only on real nand
				{
				char part[64];
				
				if (config.gameMode == GM_WII)
					{
					if (games[gamesSelected].slot < 10)
						sprintf (part, " (FAT%d)", games[gamesSelected].slot + 1);
					else
						sprintf (part, " (NTFS%d)", games[gamesSelected].slot - 10 + 1);
					}
				else
					{
					if (games[gamesSelected].slot == DEV_SD)
						sprintf (part, " (SD)");
					else
						sprintf (part, " (USB%d)", games[gamesSelected].slot);
					}
					
				strcat (info, part);
				}
			
			Video_SetFont(TTFNORM);
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, info);

			Video_SetFont(TTFSMALL);
			
			*info = '\0';
			strcat (info, "(");
			strcat (info, games[gamesSelected].asciiId);
			strcat (info, ")");
			if (config.gameMode == GM_DML)
				{
				char b[256];

				sprintf (b, " DISC %d", games[gamesSelected].disc);
				strcat (info, b);

				sprintf (b, " [%s]", games[gamesSelected].source);
				strcat (info, b);
				}

			if (strlen (info) > 60)
				{
				info[60] = 0;
				strcat (info, "...");
				}
			grlib_printf (XMIDLEINFO, theme.line1Y, GRLIB_ALIGNCENTER, 0, info);
			
			t = time(NULL);
			break;
			}
		}
	
	Video_SetFont(TTFSMALL);
	if (!grlib_irPos.valid)
		{
		if (gamesSelected == -1) grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "Point an icon with the wiimote or use a GC controller!");
		}
	else
		if (time(NULL) - t > 0 && gamesSelected == -1)
			{
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "(A) Execute (B) Settings (D-Pad) Switch mode (1) goto page (2) Filters");
			}
	
	return gamesSelected;
	}
	
static void ShowGameFilterMenu (int idx)
	{
	char title[128];
	char buff[512];
	u8 f[CATMAX];
	int i, item;

	for (i = 0; i <CATMAX; i++)
		f[i] = 0;
	
	for (i = 0; i < CATMAX; i++)
		f[i] = (games[idx].category & (1 << i)) ? 1:0;
	
	do
		{
		buff[0] = '\0';
		for (i = 0; i < CATMAX; i++) // Do not show uncat flag (always the last)
			{
			if (i == 8 || i == 16 || i == 24) grlib_menuAddColumn (buff);
			grlib_menuAddCheckItem (buff, 100 + i, f[i], flt[i]);
			}
			
		sprintf (title, "Category: %s\nPress (B) to close, (+) Select all, (-) Deselect all", games[idx].name);

		Video_SetFontMenu(TTFSMALL);
		item = grlib_menu (0, title, buff);
		Video_SetFontMenu(TTFNORM);

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
	
	games[idx].category = 0;
	for (i = 0; i < CATN; i++)
		if (f[i]) games[idx].category |= (1 << i);
	}

static bool IsFiltered (int ai)
	{
	int i;
	bool ret = false;

	if (config.gameFilter == 0) return true;
	
	for (i = 0; i < CATMAX-1; i++)
		{
		if ((config.gameFilter & (1 << i)) && (games[ai].category & (1 << i)))
			{
			ret = true;
			}
		}
		
	return ret;
	}
	
static void GetCatString (int idx, char *buff)
	{
	int i, first = 1;

	*buff = '\0';
	
	for (i = 0; i < CATN; i++)
		{
		if ((games[idx].category & (1 << i)))
			{
			if (!first)
				strcat (buff, ", ");
				
			strcat (buff, flt[i]);
			first = 0;
			}
		}
	}

static void ShowFilterMenu (void)
	{
	char buff[512];
	u8 f[CATMAX];
	int i, item;

	
	for (i = 0; i < CATMAX; i++)
		f[i] = 0;
	
	for (i = 0; i < CATMAX; i++)
		f[i] = (config.gameFilter & (1 << i)) ? 1:0;

	do
		{
		buff[0] = '\0';
		for (i = 0; i < CATMAX; i++)
			{
			if (i == 8 || i == 16 || i == 24) grlib_menuAddColumn (buff);
			grlib_menuAddCheckItem (buff, 100 + i, f[i], flt[i]);
			}

		Video_SetFontMenu(TTFSMALL);
		item = grlib_menu (0, "Filter menu\nPress (B) to close, (+) Select all, (-) Deselect all (shown all games)", buff);
		Video_SetFontMenu(TTFNORM);

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
	
	config.gameFilter = 0;
	for (i = 0; i < CATN; i++)
		if (f[i]) config.gameFilter |= (1 << i);
	
	GameBrowse (0);
	}

#define CHOPT_IOS 7
#define CHOPT_VID 8
#define CHOPT_VIDP 4
#define CHOPT_LANG 11
#define CHOPT_HOOK 8
#define CHOPT_OCA 4
#define CHOPT_NAND 5
#define CHOPT_LOADER 4
#define CHOPT_DMLVIDEOMODE 6

static void ShowAppMenu (int ai)
	{
	char buff[1024];
	char b[64];
	int item;
	
	int opt[9] = {CHOPT_IOS, CHOPT_VID, CHOPT_VIDP, CHOPT_LANG, CHOPT_HOOK, CHOPT_OCA, CHOPT_NAND, CHOPT_LOADER,CHOPT_DMLVIDEOMODE};

	char *ios[CHOPT_IOS] = { "249", "250" , "222", "223", "248", "251", "252"};
	char *nand[CHOPT_NAND] = { "Default", "USA" , "EURO", "JAP", "Korean"};
	char *loader[CHOPT_LOADER] = { "CFG", "GX", "WiiFlow", "neek2o (FAT32)"};
	char *dmlvideomode[CHOPT_DMLVIDEOMODE] = { "Auto", "Game", "WII", "NTSC", "PAL50", "PAL60"};
	/*
	char *videooptions[CHOPT_VID] = { "Default Video Mode", "Force NTSC480i", "Force NTSC480p", "Force PAL480i", "Force PAL480p", "Force PAL576i", "Force MPAL480i", "Force MPAL480p" };
	char *videopatchoptions[CHOPT_VIDP] = { "No Video patches", "Smart Video patching", "More Video patching", "Full Video patching" };
	char *languageoptions[CHOPT_LANG] = { "Default Language", "Japanese", "English", "German", "French", "Spanish", "Italian", "Dutch", "S. Chinese", "T. Chinese", "Korean" };
	char *hooktypeoptions[CHOPT_HOOK] = { "No Ocarina&debugger", "Hooktype: VBI", "Hooktype: KPAD", "Hooktype: Joypad", "Hooktype: GXDraw", "Hooktype: GXFlush", "Hooktype: OSSleepThread", "Hooktype: AXNextFrame" };
	char *ocarinaoptions[CHOPT_OCA] = { "No Ocarina", "Ocarina from NAND", "Ocarina from SD", "Ocarina from USB" };
	*/
	ReadGameConfig (ai);

	start:
	
	Video_SetFont(TTFNORM);
	gameConf.language ++; // umph... language in triiforce start at -1... not index friendly
	do
		{
		
		buff[0] = '\0';

		sprintf (b, "Played %d times##6|", games[ai].playcount);
		strcat (buff, b);

		char cat[128];
		GetCatString (ai, cat);
		if (strlen(cat) > 32)
			{
			cat[32] = 0;
			strcat (cat, "...");
			}
		if (strlen (cat) == 0)
			{
			strcpy (cat, "<none>");
			}

		strcat (buff, "Category: "); strcat (buff, cat); strcat (buff, "##5|");
		
		sprintf (b, "Vote this title (%d/10)##4||", games[ai].priority);
		strcat (buff, b);

		if (CheckParental(0))
			{
			if (games[ai].hidden)
				strcat (buff, "Remove hide flag##2|");
			else
				strcat (buff, "Hide this title ##3|");

			if (config.gameMode == GM_WII)
				{
				if (vars.neek != NEEK_NONE)
					{
					strcat (buff, "NAND: "); strcat (buff, nand[gameConf.nand]); strcat (buff, "##106|");
					}
				else
					{
					strcat (buff, "IOS: "); strcat (buff, ios[gameConf.ios]); strcat (buff, "##100|");
					strcat (buff, "Loader: "); strcat (buff, loader[gameConf.loader]); strcat (buff, "##107|");
					}
				}
			else
				{
				if (games[ai].slot == 0)
					grlib_menuAddItem (buff, 7, "Remove from SD");
					
				if (config.dmlVersion != GCMODE_DEVO)
					{
					strcat (buff, "DM(L): Video mode: "); strcat (buff, dmlvideomode[gameConf.dmlVideoMode]); strcat (buff, "##108|");
					}

				if (config.dmlVersion == GCMODE_DM22)
					   {
					   grlib_menuAddItem (buff, 8, "DM(L): Force WideScreen (%s)", gameConf.dmlNoDisc ? "Yes" : "No" );
					   grlib_menuAddItem (buff, 9, "DM(L): Full NoDisc Patching (%s)", gameConf.dmlFullNoDisc ? "Yes" : "No");
					   }

				if (config.dmlVersion == GCMODE_DML1x)
					grlib_menuAddItem (buff, 8, "DM(L): Patch NODISC (%s)", gameConf.dmlNoDisc ? "Yes" : "No" );

				if (config.dmlVersion == GCMODE_DML1x || config.dmlVersion == GCMODE_DM22)
					{
				   grlib_menuAddItem (buff,10, "DM(L): Patch PADHOOK (%s)", gameConf.dmlPadHook ? "Yes" : "No" );
				   grlib_menuAddItem (buff,11, "DM(L): Patch NMM (%s)", gameConf.dmlNMM ? "Yes" : "No" );
				   grlib_menuAddItem (buff,12, "DM(L): Enable Cheats (%s)", gameConf.ocarina ? "Yes" : "No" );
					}
					
				if (config.dmlVersion == GCMODE_DEVO)
					{
					grlib_menuAddItem (buff,13, "DEVO: Virtual Memcard ID=%d", gameConf.memcardId );
					grlib_menuAddItem (buff,14, "DEVO: widescreen (%s)", gameConf.widescreen ? "Yes" : "No" );
					grlib_menuAddItem (buff,15, "DEVO: activity led (%s)", gameConf.activity_led ? "Yes" : "No" );
					grlib_menuAddItem (buff,16, "DEVO: wifi debug (%s)", gameConf.wifi ? "Yes" : "No" );
					}
				}
			}
		/*
		else
			{
			grlib_menuAddCheckItem (buff, 108, 1 - gameConf.dmlvideomode, "NTSC mode");
			grlib_menuAddCheckItem (buff, 109, gameConf.dmlvideomode, "PAL 576i mode");
			}
		*/
		/*

		if (config.chnBrowser.nand != NAND_REAL)
			{
			
			if (gameConf.ios != MICROSNEEK_IOS)
				{
				strcat (buff, "Video: "); strcat (buff, videooptions[gameConf.vmode]); strcat (buff, "##101|");
				strcat (buff, "Video Patch: "); strcat (buff, videopatchoptions[gameConf.vpatch]); strcat (buff, "##102|");
				strcat (buff, "Language: "); strcat (buff, languageoptions[gameConf.language]); strcat (buff, "##103|");
				strcat (buff, "Hook type: "); strcat (buff, hooktypeoptions[gameConf.hook]); strcat (buff, "##104|");
				strcat (buff, "Ocarina: "); strcat (buff, ocarinaoptions[gameConf.ocarina]); strcat (buff, "##105|");
				}
			else
				{
				strcat (buff, "Video: n/a##200|");
				strcat (buff, "Video Patch: n/a##200|");
				strcat (buff, "Language: n/a##200|");
				strcat (buff, "Hook type: n/a##200|");
				strcat (buff, "Ocarina: n/a##200|");
				}
			}
		*/
		
		Video_SetFontMenu(TTFSMALL);
		item = grlib_menu (0, games[ai].name, buff);
		Video_SetFontMenu(TTFNORM);

		if (item >= 100)
			{
			int i = item - 100;
			
			if (i == 0) { gameConf.ios ++; if (gameConf.ios >= opt[i]) gameConf.ios = 0; }
			if (i == 1) { gameConf.vmode ++; if (gameConf.vmode >= opt[i]) gameConf.vmode = 0; }
			if (i == 2) { gameConf.vpatch ++; if (gameConf.vpatch >= opt[i]) gameConf.vpatch = 0; }
			if (i == 3) { gameConf.language ++; if (gameConf.language >= opt[i]) gameConf.language = 0; }
			if (i == 4) { gameConf.hook ++; if (gameConf.hook >= opt[i]) gameConf.hook = 0; }
			if (i == 5) { gameConf.ocarina ++; if (gameConf.ocarina >= opt[i]) gameConf.ocarina = 0; }
			if (i == 6) { gameConf.nand ++; if (gameConf.nand >= opt[i]) gameConf.nand = 0; }
			if (i == 7) { gameConf.loader ++; if (gameConf.loader >= opt[i]) gameConf.loader = 0; }
			if (i == 8) { gameConf.dmlVideoMode ++; if (gameConf.dmlVideoMode >= opt[i]) gameConf.dmlVideoMode = 0; }
			}
		else
			break;
		}
	while (TRUE);
	gameConf.language --;
	
	if (item == 2)
		{
		games[ai].hidden = 0;
		}

	if (item == 3)
		{
		games[ai].hidden = 1;
		}

	if (item == 4)
		{
		int item;
		item = grlib_menu (0, "Vote Game", "10 (Best)|9|8|7|6|5 (Average)|4|3|2|1 (Bad)");
		
		if (item >= 0)
			games[ai].priority = 10-item;
		}

	if (item == 5)
		{
		ShowGameFilterMenu (ai);
		goto start;
		}

	if (item == 6)
		{
		int item;
		item = grlib_menu (-60, "Reset play count ?", "Yes~No");
		
		if (item == 0)
			games[ai].playcount = 0;
		
		goto start;
		}

	if (item == 7)
		{
		int ret = grlib_menu (0, "Are you sure you want to delete this game ? ", "   YES   ~NO");
		if (ret == 0)
			{
			Debug ("Deleting folder '%s'", games[ai].source);
			fsop_KillFolderTree (games[ai].source, NULL);
			GameBrowse (1);
			}
		}
		
	if (item == 8)
		{
		gameConf.dmlNoDisc = !gameConf.dmlNoDisc;
		goto start;
		}

	if (item == 9)
		{
		gameConf.dmlFullNoDisc = !gameConf.dmlFullNoDisc;
		goto start;
		}
 
	if (item == 10)
		{
		gameConf.dmlPadHook = !gameConf.dmlPadHook;
		goto start;
		}

	if (item == 11)
		{
		gameConf.dmlNMM = !gameConf.dmlNMM;
		goto start;
		}
 
	if (item == 12)
		{
		gameConf.ocarina = !gameConf.ocarina;
		goto start;
		}
		
	if (item == 13)
		{
		gameConf.memcardId++;
		if (gameConf.memcardId >= 8) gameConf.memcardId = 0;
		goto start;
		}

	if (item == 14)
		{
		gameConf.widescreen = !gameConf.widescreen;
		goto start;
		}
		
	if (item == 15)
		{
		gameConf.activity_led = !gameConf.activity_led;
		goto start;
		}
		
	if (item == 16)
		{
		gameConf.wifi = !gameConf.wifi;
		goto start;
		}

	WriteGameConfig (ai);
	SortItems ();
	redraw = 1;
	}

static void ChangeDefaultLoader (void)
	{
	char buff[1024];
	
	buff[0] = '\0';
	
	// "CFG", "GX", "WiiFlow"
	
	grlib_menuAddItem (buff, 0, "CFG USB Loader");
	grlib_menuAddItem (buff, 1, "Usb Loader GX ");
	grlib_menuAddItem (buff, 2, "WiiFlow");
	grlib_menuAddItem (buff, 3, "neek2o (FAT32)");
	
	int item = grlib_menu (0, "Select your default loader", buff);
	if (item >= 0)
		config.gameDefaultLoader = item;
		
	Redraw();
	grlib_PushScreen();

	int i;
	for (i = 0; i < gamesCnt; i++)
		{
		int ret = ReadGameConfig (i);
		if (ret > 0)
			{
			gameConf.loader = config.gameDefaultLoader;
			WriteGameConfig (i);
			}
		
		Video_WaitPanel (TEX_HGL, "Updating configuration files|%d%%", (i * 100)/gamesCnt);
		}
	}

static void ChangeDefaultDMLMode (void)
	{
	char buff[1024];
	
	buff[0] = '\0';
	
	// "CFG", "GX", "WiiFlow"
	
	grlib_menuAddItem (buff, 0, "From game ID");
	grlib_menuAddItem (buff, 1, "WII current configuration");
	grlib_menuAddItem (buff, 2, "Force NTSC");
	grlib_menuAddItem (buff, 3, "Force PAL");
	
	int item = grlib_menu (0, "Select your default DML Video mode", buff);
	if (item >= 0)
		{
		config.gameDefaultLoader = item;
			
		Redraw();
		grlib_PushScreen();

		int i;
		for (i = 0; i < gamesCnt; i++)
			{
			int ret = ReadGameConfig (i);

			if (ret > 0)
				{
				gameConf.dmlVideoMode = item;
				//Debug ("(update)ReadGameConfig %d, %d", i, ret);
				WriteGameConfig (i);
				}
			
			Video_WaitPanel (TEX_HGL, "Updating configuration files|%d%%", (i * 100)/gamesCnt);
			}
		}
	}

static void ShowMainMenu (void)
	{
	char buff[512];
	int rebrowse = 0;
	
start:
	
	buff[0] = '\0';
	
	if (CheckParental(0))
		{
		if (vars.neek != NEEK_NONE)
			{
			if (config.gameMode == GM_WII)
				grlib_menuAddItem (buff, 5, "Refresh game cache (reboot)...");
			else
				grlib_menuAddItem (buff, 6, "Refresh game cache...");
			}
		else
			{
			grlib_menuAddItem (buff, 6, "Refresh game cache...");
			}

		grlib_menuAddItem (buff, 1, "Download covers...");
		}

	if (config.gameSort == 0)
		grlib_menuAddItem (buff,  8, "Sort by: vote");
	if (config.gameSort == 1)
		grlib_menuAddItem (buff,  8, "Sort by: name");
	if (config.gameSort == 2)
		grlib_menuAddItem (buff,  8, "Sort by: play count");
	
	grlib_menuAddItem (buff,  9, "Select filters");

	if (CheckParental(0))
		{
		if (config.gameMode == GM_WII)
			{
			if (vars.neek == NEEK_NONE) grlib_menuAddItem (buff, 2, "Set default loader...");
			}
		else
			{
			if (config.dmlVersion == GCMODE_DML0x)
				grlib_menuAddItem (buff, 12, "GameCube mode: DML v0.x");
			if (config.dmlVersion == GCMODE_DML1x)
				grlib_menuAddItem (buff, 12, "GameCube mode: DML v1.x");
			if (config.dmlVersion == GCMODE_DM22)
				grlib_menuAddItem (buff, 12, "GameCube mode: DM v2.2+ USB");
			if (config.dmlVersion == GCMODE_DEVO)
				grlib_menuAddItem (buff, 12, "GameCube mode: Devolution");

			if (config.dmlVersion != GCMODE_DEVO)
				grlib_menuAddItem (buff, 3, "Set default videomode...");
			}
		
		if (showHidden)
			grlib_menuAddItem (buff, 10, "Hide hidden games");
		else
			grlib_menuAddItem (buff, 11, "Show hidden games");
		}
					
	Redraw();
	grlib_PushScreen();
	
	int item = grlib_menu (0, "Games menu", buff);
	
	if (item == 1)
		{
		DownloadCovers();
		}

	if (item == 2)
		{
		ChangeDefaultLoader ();
		}

	if (item == 3)
		{
		ChangeDefaultDMLMode ();
		}
	
	if (item == 4)
		{
		neek_CreateCDIConfig (NULL);
		Shutdown ();
		SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
		}

	if (item == 5)
		{
		neek_KillDIConfig ();
		Shutdown ();
		SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
		}

	if (item == 6)
		{
		GameBrowse (1);
		}
		
	if (item == 8)
		{
		config.gameSort ++;
		if (config.gameSort > 2) config.gameSort = 0;
		goto start;
		}

	if (item == 9)
		{
		ShowFilterMenu ();
		goto start;
		}
		
	if (item == 10)
		{
		showHidden = 0;
		redraw = 1;
		}

	if (item == 11)
		{
		showHidden = 1;
		redraw = 1;
		}
		
	if (item == 12)
		{
		config.dmlVersion++;
		if (config.dmlVersion >= GCMODE_MAX)
			config.dmlVersion = GCMODE_DML0x;
			
		rebrowse = 1;
		goto start;
		}
			
	if (rebrowse)
		GameBrowse (1);
	else
		SortItems ();
	
	}

static void RedrawIcons (int xoff, int yoff)
	{
	int i;
	int ai;	// Application index (corrected by the offset)
	char path[128];

	//GRRLIB_PrintfTTF_Debug (1);

	Video_SetFont(TTFSMALL);
	
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
		
		if (ai < gamesCnt && ai < games2Disp && gui.spotsIdx < SPOTSMAX)
			{
			MakeCoverPath (ai, path);
			gui.spots[gui.spotsIdx].ico.icon = CoverCache_Get(path);
				
			if (!gui.spots[gui.spotsIdx].ico.icon)
				{
				char title[256];
				strcpy (title, games[ai].name);
				title[48] = 0;
				strcpy (gui.spots[gui.spotsIdx].ico.title, title);
				}
			else
				gui.spots[gui.spotsIdx].ico.title[0] = '\0';

			if (config.gameMode == GM_DML)
				{
				if (config.dmlVersion < GCMODE_DM22 && games[ai].slot)
					gui.spots[gui.spotsIdx].ico.transparency = 128;
				else
					gui.spots[gui.spotsIdx].ico.transparency = 255;
				}
				
			// Is it hidden ?
			if (games[ai].hidden && showHidden)
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = vars.tex[TEX_GHOST];
			else
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = 0;
				
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
	}

static void Redraw (void)
	{
	
	Video_DrawBackgroud (1);
	
	Video_SetFont(TTFNORM);
	if (config.gameMode == GM_WII)
		grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader::WII Games");
	else
		{
		char buff[32];
		
		if (config.dmlVersion == GCMODE_DML0x) strcpy (buff, "DML 0.X");
		if (config.dmlVersion == GCMODE_DML1x) strcpy (buff, "DML 1.X");
		if (config.dmlVersion == GCMODE_DM22) strcpy (buff, "DM 2.2+");
		if (config.dmlVersion == GCMODE_DEVO) strcpy (buff, "Devolution");
		
		grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader::GameCube Games (%s)", buff);
		}
	
	grlib_printf ( 615, 26, GRLIB_ALIGNRIGHT, 0, "Page %d of %d", page+1, pageMax+1);
	
	if (redrawIcons) RedrawIcons (0,0);

	Video_DrawVersionInfo ();

	if (gamesCnt == 0 && scanned)
		{
		Video_SetFont(TTFNORM);
		grlib_DrawCenteredWindow ("No games found, please rebuild the cache!", WAITPANWIDTH, 133, 0, NULL);
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
	
	GRRLIB_SetFBMode (0); // Enable double fbmode
	
	return page;
	}

static void cb_filecopy (void)
	{
	u32 mb = (u32)(fsop.multy.bytes/1000000);
	u32 sizemb = (u32)(fsop.multy.size/1000000);
	u32 perc = (mb * 100)/sizemb;
	
	Video_WaitPanel (TEX_HGL, "Please wait: %u%% done|Copying %u of %u Mb (%u Kb/sec)", perc, mb, sizemb, (u32)(fsop.multy.bytes/fsop.multy.elapsed));
	
	if (grlib_GetUserInput() & WPAD_BUTTON_B)
		{
		int ret = grlib_menu (0, "This will interrupt the copy process... Are you sure", "   Yes   ##0~No##-1");
		if (ret == 0) fsop.breakop = 1;
		}
	}

static size_t GetGCGameUsbKb (int ai)
	{
	//char path[300];
	
	//sprintf (path, "%s://ngc/%s",  devices_Get (games[ai].slot), games[ai].asciiId);
	
	Debug ("GetGCGameUsbKb %s", games[ai].source);
	return fsop_GetFolderKb (games[ai].source, NULL);
	}

static bool MoveGCGame (int ai)
	{
	char source[300];
	char target[300];
	
	snd_Pause ();
	DMLResetCache ();

	Debug ("MoveGCGame (%s %s '%s'): Started", games[ai].name, games[ai].asciiId, games[ai].source);

	//sprintf (source, "%s://ngc/%s", devices_Get (games[ai].slot), games[ai].asciiId);
	sprintf (source, "%s", games[ai].source);
	
	//sprintf (target, "sd://games/%s", games[ai].asciiId);
	char *p;
	p = games[ai].source + strlen(games[ai].source) - 1;
	while (*p != '/') p--;
	p++;
	sprintf (target, "%s://games/%s", devices_Get (DEV_SD), p);
	
	Debug ("MoveGCGame: '%s' -> '%s'", source, target);

	bool ret = fsop_CopyFolder (source, target, cb_filecopy);
	
	if (!ret || fsop.breakop)
		{
		Debug ("MoveGCGame: interrupted... removing partial folder");
		fsop_KillFolderTree (target, NULL);
		}
		
	snd_Resume ();
	
	return ret;
	}

static bool QuerySelection (int ai)
	{
	int i;
	float mag = 1.0;
	float magMax = 2.2;
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
	MakeCoverPath (ai, path);
	mt_Lock();
	GRRLIB_texImg * tex = GRRLIB_LoadTextureFromFile (path);
	mt_Unlock();
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

		DrawInfoWindo (yInf, games[ai].name, "Press (A) to start, (B) Cancel");
		yInf -= 5;
		if (yInf < 400) yInf = 400;

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
		grlib_DrawSquare (&black);
		grlib_IconDraw (&istemp, &ico);
		Overlay ();
		
		DrawInfoWindo (yInf, games[ai].name, "Press (A) to start, (B) Cancel");
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

static void Conf (bool open)
	{
	char cfgpath[64];
	sprintf (cfgpath, "%s://ploader/games.conf", vars.defMount);
	
	Video_WaitIcon (TEX_HGL);
	
	cfg_Section (NULL);
	if (open)
		{
		mt_Lock();
		cfg = cfg_Alloc (cfgpath, GAMEMAX, 0, 0);
		mt_Unlock();
		}
	else
		{
		mt_Lock();
		cfg_Store (cfg, cfgpath);
		mt_Unlock();
		cfg_Free (cfg);
		}
	}
		
int GameBrowser (void)
	{
	u32 btn;
	int gameModeLast = config.gameMode;

	Debug ("GameBrowser (begin)");
	
	Conf (true);

	page = 0;
	scanned = 0;
	browserRet = -1;

	grlibSettings.color_window = RGBA(192,192,192,255);
	grlibSettings.color_windowBg = RGBA(32,32,32,128);
	
	grlib_SetRedrawCallback (Redraw, Overlay);
	
	games = calloc (GAMEMAX, sizeof(s_game));
	
	// Immediately draw the screen...
	StructFree ();
	InitializeGui ();
	
	/*
	Redraw ();
	grlib_PushScreen ();
	grlib_PopScreen ();
	grlib_Render();  // Render the theme.frame buffer to the TV
	*/
	
	page = config.gamePageWii;
	GameBrowse (0);

	LiveCheck (1);

	if (config.gameMode == GM_WII)
		page = config.gamePageWii;
	else
		page = config.gamePageGC;

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
			
			if (btn & WPAD_BUTTON_A && gamesSelected != -1) 
				{
				if (!QuerySelection (gamesSelected))
					{
					redraw = 1;
					continue;
					}
				if (config.gameMode == GM_WII)
					{
					ReadGameConfig (gamesSelected);
					games[gamesSelected].playcount++;
					WriteGameConfig (gamesSelected);
					
					memcpy (&config.run.game, &gameConf, sizeof(s_gameConfig));
					config.run.neekSlot = games[gamesSelected].slot;
					strcpy (config.run.asciiId, games[gamesSelected].asciiId);
					
					browserRet = INTERACTIVE_RET_GAMESEL;
					}
				else
					{
					bool err = false;
					
					Debug ("gamebrowser: requested dml");

					if (config.dmlVersion < GCMODE_DM22 && games[gamesSelected].slot)
						{
						int ret = DMLInstall (games[gamesSelected].name, GetGCGameUsbKb(gamesSelected));

						Redraw();
						grlib_PushScreen();

						if (ret)
							err = !MoveGCGame (gamesSelected);
						else
							err = true;
						}
					
					Debug ("gamebrowser: requested dml (err = %d)", err);

					if (!err)
						{
						MasterInterface (1, 0, 3, "Booting...");

						Debug ("DMLRun");
						ReadGameConfig (gamesSelected);
						games[gamesSelected].playcount++;
						WriteGameConfig (gamesSelected);
						Conf (false);	// Store configuration on disc
						config.gamePageGC = page;
						
						//Debug ("%s -> %s", games[gamesSelected].source, p);
						
						// Execute !
						
						switch (config.dmlVersion)
							{
							case GCMODE_DML0x:
								{
								// Retrive the folder name
								char *p = games[gamesSelected].source + strlen(games[gamesSelected].source) - 1;
								while (*p != '/') p--;
								p++;

								DMLRun (p, games[gamesSelected].asciiId, gameConf.dmlVideoMode);
								}
								break;
							
							case GCMODE_DML1x:
							case GCMODE_DM22:
								{
								char *p = strstr (games[gamesSelected].source, "//")+1;
								DMLRunNew (p, games[gamesSelected].asciiId, &gameConf);
								}
								break;
								
							case GCMODE_DEVO:
								{
								DEVO_Boot (games[gamesSelected].source, gameConf.memcardId, gameConf.widescreen, gameConf.activity_led, gameConf.wifi);
								}
								break;
							}
						
						Video_SetFont(TTFNORM);
						grlib_menu (0, "There was a problem executing DML.\n\nPlease check if 'GameCube mode' is the right one. Press [home]", "   OK   ");
						Conf (true);	// Store configuration on disc
						}
					
					redraw = 1;
					}
				}
				
			/////////////////////////////////////////////////////////////////////////////////////////////
			// Select application as default
			if (btn & WPAD_BUTTON_B && gamesSelected != -1)
				{
				ShowAppMenu (gamesSelected);
				redraw = 1;
				}

			if (btn & WPAD_BUTTON_1) 
				{
				if (gamesCnt == 0) continue;

				page = GoToPage (page, pageMax);
				FeedCoverCache ();
				redraw = 1;
				}
			
			if (btn & WPAD_BUTTON_2)
				{
				if (gamesCnt == 0) continue;
				
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
			GameBrowse (0);
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

	// save current page
	if (gameModeLast == GM_WII)
		config.gamePageWii = page;
	else
		config.gamePageGC = page;
	
	Conf (false);

	// Clean up all data
	StructFree ();
	gui_Clean ();
	free (games);

	grlib_SetRedrawCallback (NULL, NULL);
	
	return browserRet;
	}