#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <dirent.h>
#include "wiiload/wiiload.h"
#include "globals.h"
#include "bin2o.h"
#include "channels.h"
#include "network.h"
#include "sys/errno.h"
#include "http.h"
#include "ios.h"
#include "identify.h"
#include "gui.h"
#include "mystring.h"
#include "cfg.h"
#include "devices.h"
#include "neek.h"
#include "isfs.h"
#include "browser.h"

#define CHNMAX 1024

/*

This allow to browse for applications in chans folder of mounted drive

*/
extern s_grlibSettings grlibSettings;
static s_channelConfig chnConf;

static int browse = 0;

static s_channel *chans;
static int chansCnt;
static int chans2Disp;
static int page; // Page to be displayed
static int pageMax; // 
static int chansSelected = -1;	// Current selected app with wimote

static int browserRet = 0;
static int showHidden = 0;
static int manageUID = 0;

static u8 redraw = 1;
static bool redrawIcons = true;

static s_grlib_iconSetting is;

static void Redraw (void);
static int ChnBrowse (void);

static bool nandScanned = false;
static int disableSpots;

static s_cfg *cfg;

#define ICONW 128
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
	gui.spotsXpage = 12;
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
	
static void Conf (bool open)
	{
	char cfgpath[64];
	
	Debug ("Conf %d", open);
	
	if (vars.neek != NEEK_NONE)
		sprintf (cfgpath, "%s://ploader/chneek.conf", vars.defMount);
	else if (config.chnBrowser.nand == NAND_REAL)
		sprintf (cfgpath, "%s://ploader/chreal.conf", vars.defMount);
	else
		sprintf (cfgpath, "%s://ploader/chemul.conf", vars.defMount);

	cfg_Section (NULL);
	if (open)
		{
		cfg = cfg_Alloc (cfgpath, CHNMAX, 0, 0);
		}
	else
		{
		cfg_Store (cfg, cfgpath);
		cfg_Free (cfg);
		}
	}
	
static bool DownloadCovers_Get (char *path, char *buff)
	{
	u8* outbuf=NULL;
	u32 outlen=0;
	
	outbuf = downloadfile (buff, &outlen, NULL);
		
	if (IsPngBuff (outbuf, outlen))
		{
		//suficientes bytes
		FILE *f;
		
		f = fopen (path, "wb");
		if (f)
			{
			fwrite (outbuf, outlen, 1, f);
			fclose (f);
			}
		
		free(outbuf);
		return (TRUE);
		}
		
	return (FALSE);
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
		
		if (ai >= 0 && ai < chansCnt && chans[ai].hasCover)
			{
			sprintf (path, "%s://ploader/covers/%s.png", vars.defMount, chans[ai].asciiId);
			CoverCache_Add (path,  (i >= 0 && i < gui.spotsXpage) ? true:false );
			}
		}

	CoverCache_Pause (false);
	}

static void DownloadCovers (void)
	{
	int ia, ireg, stop;
	char buff[300];
	const char *regions[4]={"EN", "US", "JA", "other" };
	char path[PATHMAX];
	
	Redraw ();
	grlib_PushScreen ();
	
	Video_WaitPanel (TEX_HGL, "Stopping wiiload thread");
	WiiLoad_Pause ();
	
	stop = 0;
	
	FILE *f = NULL;
	if (devices_Get (DEV_SD))
		{
		sprintf (path, "%s://misschan.txt", devices_Get(DEV_SD));
		f = fopen (path, "wb");
		}
	
	for (ia = 0; ia < chansCnt; ia++)
		{
		Video_WaitPanel (TEX_HGL, "Downloading %s.png (%d of %d)|(B) Stop", chans[ia].asciiId, ia, chansCnt);
		sprintf (path, "%s://ploader/covers/%s.png", vars.defMount, chans[ia].asciiId);

		if (!fsop_FileExist(path))
			{
			bool ret = FALSE;
		
			if (!ret)
				{
				sprintf (buff, "http://www.postloader.freehosting.com/png/%s.png", chans[ia].asciiId);
				ret = DownloadCovers_Get (path, buff);
				}

			for (ireg = 0; ireg < 4; ireg ++)
				{
				if (!ret)
					{
					sprintf (buff, "http://art.gametdb.com/wii/cover/%s/%s.png", regions[ireg], chans[ia].asciiId);
					ret = DownloadCovers_Get (path, buff);
					}
				}
				
			if (!ret && f)
				{
				sprintf (buff, "%s:%s\n", chans[ia].asciiId, chans[ia].name);
				fwrite (buff, 1, strlen(buff), f);
				}
				
			if (grlib_GetUserInput () == WPAD_BUTTON_B)
				{
				stop = 1;
				break;
				}
			}
		if (stop) break;
		}
	
	if (f) fclose (f);
	
	WiiLoad_Resume ();
	
	CoverCache_Flush ();
	
	ChnBrowse ();
	}
	
static void WriteTitleConfig (int ia)
	{
	if (ia >= 0)
		{
		chnConf.titleId = chans[ia].titleId;
		chnConf.hidden = chans[ia].hidden;
		chnConf.priority = chans[ia].priority;
		strcpy (chnConf.asciiId, chans[ia].asciiId);
		}
	
	char buff[2048];
	int index = 0;
	*buff = '\0';
	
	cfg_Section (NULL);
	u32 th = TITLE_UPPER(chnConf.titleId);
	u32 tl = TITLE_LOWER(chnConf.titleId);
	cfg_FmtString (buff, CFG_WRITE, CFG_U32, &th, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U32, &tl, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_INT, &chnConf.priority, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8,  &chnConf.hidden, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8,  &chnConf.ios, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8,  &chnConf.vmode, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_S8,  &chnConf.language, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8,  &chnConf.vpatch, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8,  &chnConf.hook, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8,  &chnConf.ocarina, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U8,  &chnConf.bootMode, index++);
	cfg_FmtString (buff, CFG_WRITE, CFG_U16, &chnConf.playcount, index++);

	cfg_SetString (cfg, chnConf.asciiId, buff);
	}

static bool ReadTitleConfig (int ia)
	{
	char buff[2048];
	bool valid;
	
	valid = (cfg_GetString (cfg, chans[ia].asciiId, buff) != -1 && cfg_CountSepString (buff) >= 12);
	
	if (valid)
		{
		int index = 0;
		cfg_Section (NULL);
		u32 th, tl;
		cfg_FmtString (buff, CFG_READ, CFG_U32, &th, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U32, &tl, index++);
		chnConf.titleId = TITLE_ID(th, tl);
		cfg_FmtString (buff, CFG_READ, CFG_INT, &chnConf.priority, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8,  &chnConf.hidden, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8,  &chnConf.ios, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8,  &chnConf.vmode, index++);
		cfg_FmtString (buff, CFG_READ, CFG_S8,  &chnConf.language, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8,  &chnConf.vpatch, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8,  &chnConf.hook, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8,  &chnConf.ocarina, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U8,  &chnConf.bootMode, index++);
		cfg_FmtString (buff, CFG_READ, CFG_U16, &chnConf.playcount, index++);

		chans[ia].titleId = chnConf.titleId;
		chans[ia].hidden = chnConf.hidden;
		chans[ia].priority = chnConf.priority;
		}
	
	return valid;
	}

static void ChansFree (void)
	{
	int i;
	
	for (i = 0; i < chansCnt; i++)
		{
		chans[i].png = NULL;
		
		if (chans[i].name != NULL) 
			{
			free (chans[i].name);
			chans[i].name = NULL;
			}
		}
		
	chansCnt = 0;
	}
	
static bool CheckFilter (int ai)
	{
	int i;
	
	for (i = 0; i < CHANNELS_MAXFILTERS-1; i++)
		{
		if (config.chnBrowser.filter[i] && chans[ai].asciiId[0] == CHANNEL_FILTERS[i])
			return TRUE;
		}
		
	// Ok, title is not found, but maybe is an OTHER title
	if (config.chnBrowser.filter[CHANNELS_MAXFILTERS-1])
		{
		for (i = 0; i < CHANNELS_MAXFILTERS-1; i++)
			if (chans[ai].asciiId[0] == CHANNEL_FILTERS[i])
				return FALSE;
			
		return TRUE;
		}
		
	return FALSE;
	}

static int qsort_name (const void * a, const void * b)
	{
	s_channel *aa = (s_channel*) a;
	s_channel *bb = (s_channel*) b;
	
	if (!aa->name || !bb->name) return 0;
	
	return (ms_strcmp (aa->name, bb->name));
	}

static int bsort_filtered (const void * a, const void * b)
	{
	s_channel *aa = (s_channel*) a;
	s_channel *bb = (s_channel*) b;

	if (aa->filtered < bb->filtered) return 1;
	
	return 0;
	}

static int bsort_hidden (const void * a, const void * b)
	{
	s_channel *aa = (s_channel*) a;
	s_channel *bb = (s_channel*) b;

	if (aa->hidden > bb->hidden) return 1;
	
	return 0;
	}

static int bsort_priority (const void * a, const void * b)
	{
	s_channel *aa = (s_channel*) a;
	s_channel *bb = (s_channel*) b;

	if (aa->priority < bb->priority) return 1;
	
	return 0;
	}

static void SortItems (void)
	{
	int i;
	int filtered = 0;
	
	// Apply filters
	if (manageUID)
		{
		chans2Disp = chansCnt;
		}
	else
		{
		chans2Disp = 0;
		for (i = 0; i < chansCnt; i++)
			{
			chans[i].filtered = CheckFilter(i);
			if (chans[i].filtered && (!chans[i].hidden || showHidden)) chans2Disp++;
			if (chans[i].filtered) filtered++;
			}
		}
	
	bsort (chans, chansCnt, sizeof(s_channel), bsort_filtered);
	bsort (chans, filtered, sizeof(s_channel), bsort_hidden);
	qsort (chans, chans2Disp, sizeof(s_channel), qsort_name);
	bsort (chans, chans2Disp, sizeof(s_channel), bsort_priority);

	pageMax = (chans2Disp-1) / gui.spotsXpage;
	
	FeedCoverCache ();
	}
	
static void GetCacheFileName (char *path)
	{
	if (vars.neek == NEEK_NONE)
		{
		if (config.chnBrowser.nand == NAND_REAL)
			sprintf (path, "%s://ploader/nand.dat", vars.defMount);
		if (config.chnBrowser.nand == NAND_EMUSD)
			sprintf (path, "sd:/%s/nand.dat", config.chnBrowser.nandPath);
		if (config.chnBrowser.nand == NAND_EMUUSB)
			sprintf (path, "usb:/%s/nand.dat", config.chnBrowser.nandPath);
		}
	else
		{
		if (strlen(vars.neekName))
			{
			int i;
			char p[300];
			
			strcpy (p, vars.neekName);
			for (i = 0; i < strlen(p); i++)
				if (p[i] == '/')
					p[i] = '_';
					
			sprintf (path, "%s://ploader/%s.dat", vars.defMount, p);
			}
		else
			sprintf (path, "%s://ploader/nandneek.dat", vars.defMount);
		}
	
	Debug ("GetCacheFileName %s", path);
	}

// This will check if cover is available
static void CheckForCovers (void)
	{
	DIR *pdir;
	struct dirent *pent;

	char path[256];
	int i;
	
	// Cleanup cover flag
	for (i = 0; i < chansCnt; i++)
		chans[i].hasCover = 0;
		
	sprintf (path, "%s://ploader/covers", vars.defMount);
	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		// Skip it
		if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0 || ms_strstr (pent->d_name, ".png") == NULL)
			continue;
			
		for (i = 0; i < chansCnt; i++)
			{
			if (!chans[i].hasCover && ms_strstr (pent->d_name, chans[i].asciiId))
				chans[i].hasCover = 1;
			}
		}
	closedir(pdir);
	}


/*
To have the maximum speed, titles are now stored in cache file. Cache file must be rebuilded 
*/

static int RebuildCacheFile (void)
	{
	int i, cnt;
	int ret;
	int id;
	
	char path[PATHMAX];
	char *names[CHNMAX];
	u32 upper[CHNMAX], lower[CHNMAX];
	
	u64 *TitleIds;
	u32 Titlecount;
	
	Debug ("RebuildCacheFile()");
	
	nandScanned = false;
	
	Redraw ();
	grlib_PushScreen ();
	
#ifndef DOLPHINE
	if (config.chnBrowser.nand == NAND_REAL && vars.neek == NEEK_NONE)
		{
		if (vars.ios == IOS_PREFERRED)
			{
			Debug ("RebuildCacheFile: Patching NAND permission");
			Video_WaitPanel (TEX_CHIP, "Patching NAND permission");
			sleep (1);
			IOSPATCH_Apply ();
			if (IOSTATCH_Get (PATCH_ISFS_PERMISSIONS) == 0)
				{
				grlib_menu (0, "postLoader was unable to patch ISFS permission. Cannot continue", "  OK  ");
				Debug ("postLoader was unable to patch ISFS permission. Cannot continue");
				return -1;
				}
			}
		else if (vars.ios == IOS_CIOS)
			{
			Video_WaitPanel (TEX_CHIP, "Please wait...");
			}
		else
			{
			grlib_menu (0, "Unsupported ios.\npostLoader need 58+AHPBROT or cIOSX rev21d2x on slot 249.\nCannot continue", "  OK  ");
			return -1;
			}
		}
#endif

	if (config.chnBrowser.nand == NAND_REAL) ISFS_Initialize();

	*path = '\0';
	if (config.chnBrowser.nand != NAND_REAL)
		{
		if (config.chnBrowser.nand == NAND_EMUSD) 
			sprintf (path, "sd:/%s", config.chnBrowser.nandPath);
		if (config.chnBrowser.nand == NAND_EMUUSB)
			sprintf (path, "usb:/%s", config.chnBrowser.nandPath);
		
		Debug ("RebuildCacheFile: browsing '%s'", path);
		Video_WaitPanel (TEX_CHIP, "Please wait...");
		}

	LoadTitlesTxt ();
	
	cnt = 0;
	for (id = 0; id < 2; id++)
		{
		if (*path == 0)
			ret = get_game_list(&TitleIds, &Titlecount, id);
		else
			ret = get_game_listEmu(&TitleIds, &Titlecount, id, path);
			
		Debug ("get_game_list: %d", ret);
		
		if (ret > 0)
			{
			for (i = 0; i < Titlecount; i++)
				{
				lower[cnt] = TITLE_LOWER(TitleIds[i]);
				upper[cnt] = TITLE_UPPER(TitleIds[i]);
				
				// Search title in cache
				names[cnt] = get_name(TitleIds[i], path);
				
				char asciiId[6];
				memset(asciiId, 0, 6);
				memcpy(asciiId, (char *)(&lower[cnt]), 4);
				
				char *title = cfg_FindInBuffer (vars.titlestxt, asciiId);
				
				//Debug ("RebuildCacheFile: found '%s'", names[cnt]);
				//Debug ("%s -> %s (%s)", asciiId, title == NULL ? "(null)" : title, names[cnt]);
				if (title)
					{
					free (names[cnt]);
					names[cnt] = ms_utf8_to_ascii (title);
					}
				
				Video_WaitPanel (TEX_HGL, "Searching for channels: %d found...", cnt);
				
				cnt++;
				}
			
			free (TitleIds);
			}
		}
		
	free (vars.titlestxt);
	vars.titlestxt = NULL;

	if (config.chnBrowser.nand == NAND_REAL) ISFS_Deinitialize();

	GetCacheFileName (path);
	
	Debug ("RebuildCacheFile: storing cache '%s'", path);
	
	FILE* f = NULL;
	char buff[256];
	
	f = fopen(path, "wb");
	if (!f) 
		{
		Debug ("RebuildCacheFile: failed to open '%s'", path);
		return 0;
		}
	
	for (i = 0; i < cnt; i++)
		{
		sprintf (buff, "%u:%u:%s\n", upper[i], lower[i], names[i]);
		fwrite (buff, 1, strlen(buff), f );
		
		free (names[i]);
		}
	fclose(f);
	
	Debug ("RebuildCacheFile: done !");
	
	return cnt;
	}

static int ChnBrowse (void)
	{
	int i;
	char path[PATHMAX];
	char *titles;
	char *p;
	char name[128];
	u32 lower, upper;
	size_t size;
	
	Debug ("ChnBrowse: begin!");
	
	gui.spotsIdx = 0;
	gui_Clean ();
	ChansFree ();

	// Load the right cache file
	GetCacheFileName (path);
	
	titles = (char*)fsop_ReadFile (path, 0, &size);
	if (!titles)
		{
		Debug ("ChnBrowse: fsop_ReadFile failed '%s'", path);
		return 0;
		}
	
	chansCnt = 0;
	p = titles;
	while (p != NULL && *p != '\0')
		{
		// get ascii id
		upper = atol(p);
		while (*p != '\0' && *p != ':')
			{
			p++;
			}
		p++;
		lower = atol(p);

		while (*p != '\0' && *p != ':')
			{
			p++;
			}
		p++;
				
		// get ascii id
		memset (name, 0, sizeof(name));
		while (*p <= 32) p++;
		for (i = 0; i < 63; i++)
			{
			if (*p == '\n' || *p == '\0') 
				break;
			else
				name[i] = *p;
			
			p++;
			}
		p++;
		
		if (upper > 0 && lower > 0 && strlen (name) >= 4)
			{
			chans[chansCnt].titleId = TITLE_ID (upper, lower);
			memset(chans[chansCnt].asciiId, 0, 6);
			memcpy(chans[chansCnt].asciiId, (char *)(&lower), 4);

			// Search title in cache
			chans[chansCnt].name = malloc (strlen(name)+1);
			if (chans[chansCnt].name)
				strcpy (chans[chansCnt].name, name);

			chans[chansCnt].png = NULL;
			// Update configuration
			if (!ReadTitleConfig (chansCnt))
				{
				sprintf (chnConf.asciiId, chans[chansCnt].asciiId);
				chnConf.priority = 5;
				chnConf.hidden = 0;
				chnConf.titleId = chans[chansCnt].titleId;
				chnConf.ios = 0;
				chnConf.vmode = 0;
				chnConf.language = -1;
				chnConf.vpatch = 0;
				chnConf.ocarina = 0;
				chnConf.hook = 0;
				
				if (vars.neek != NEEK_NONE || config.chnBrowser.nand == NAND_REAL)
					chnConf.bootMode = 2; // Compatible
				else 
					chnConf.bootMode = 0; // normal
					
				chnConf.playcount = 0;
				
				WriteTitleConfig (-1);
				}
			
			if (chansCnt % 20 == 0) Video_WaitIcon (TEX_HGL);

			chansCnt++;
			}
		}
		
	nandScanned = true;
	
	free (titles);
	
	CheckForCovers ();
	SortItems ();
	
	Debug ("ChnBrowse: done!");
	return chansCnt;
	}

static int FindSpot (void)
	{
	int i,j;
	static time_t t = 0;
	char info[300];
	
	Video_SetFont(TTFNORM);
	
	chansSelected = -1;
	
	if (disableSpots) return chansSelected;
	
	for (i = 0; i < gui.spotsIdx; i++)
		{
		if (grlib_irPos.x > gui.spots[i].ico.rx1 && grlib_irPos.x < gui.spots[i].ico.rx2 && grlib_irPos.y > gui.spots[i].ico.ry1 && grlib_irPos.y < gui.spots[i].ico.ry2)
			{
			// Ok, we have the point
			chansSelected = gui.spots[i].id;

			gui.spots[i].ico.sel = true;
			grlib_IconDraw (&is, &gui.spots[i].ico);

			Video_SetFont(TTFNORM);
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, chans[chansSelected].name);
			
			Video_SetFont(TTFSMALL);
			
			*info = '\0';
			for (j = 0; j < CHANNELS_MAXFILTERS; j++)
				if (*chans[chansSelected].asciiId == *CHANNELS_NAMES[j])
					{
					sprintf (info, "%s ", &CHANNELS_NAMES[j][1]);
					break;
					}

			strcat (info, "(");
			strcat (info, chans[chansSelected].asciiId);
			strcat (info, ")");
			
			grlib_printf (XMIDLEINFO, theme.line1Y, GRLIB_ALIGNCENTER, 0, info);
			
			t = time(NULL);
			break;
			}
		}
	
	Video_SetFont(TTFSMALL);
	if (!grlib_irPos.valid)
		{
		if (chansSelected == -1) grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "Point an icon with the wiimote or use a GC controller!");
		}
	else
		if (time(NULL) - t > 0 && chansSelected == -1)
			{
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "(A) Execute (B) Settings (D-Pad) Switch mode (1) goto page (2) Filters");
			}
	
	return chansSelected;
	}
	
#define CHOPT_IOS 6
#define CHOPT_VID 8
#define CHOPT_VIDP 4
#define CHOPT_LANG 11
#define CHOPT_HOOK 8
#define CHOPT_OCA 4
#define CHOPT_BOOT 3

static void ShowAppMenu (int ai)
	{
	char buff[1024];
	char b[64];
	int item;
	
	int opt[7] = {CHOPT_IOS, CHOPT_VID, CHOPT_VIDP, CHOPT_LANG, CHOPT_HOOK, CHOPT_OCA, CHOPT_BOOT};
	
	char *ios[CHOPT_IOS] = { "249", "250", "251", "252", "247" , "248"};
	char *videooptions[CHOPT_VID] = { "Default Video Mode", "Force NTSC480i", "Force NTSC480p", "Force PAL480i", "Force PAL480p", "Force PAL576i", "Force MPAL480i", "Force MPAL480p" };
	char *videopatchoptions[CHOPT_VIDP] = { "No Video patches", "Smart Video patching", "More Video patching", "Full Video patching" };
	char *languageoptions[CHOPT_LANG] = { "Default Language", "Japanese", "English", "German", "French", "Spanish", "Italian", "Dutch", "S. Chinese", "T. Chinese", "Korean" };
	char *hooktypeoptions[CHOPT_HOOK] = { "No Ocarina&debugger", "Hooktype: VBI", "Hooktype: KPAD", "Hooktype: Joypad", "Hooktype: GXDraw", "Hooktype: GXFlush", "Hooktype: OSSleepThread", "Hooktype: AXNextFrame" };
	char *ocarinaoptions[CHOPT_OCA] = { "No Ocarina", "Ocarina from NAND", "Ocarina from SD", "Ocarina from USB" };
	char bootmethodoptions[CHOPT_BOOT][32] = { "Normal boot method", "Load apploader", "neek2o" };
	
	if (vars.neek != NEEK_NONE || config.chnBrowser.nand == NAND_REAL)
		strcpy (bootmethodoptions[2], "Compatible mode");
	
	Video_SetFont(TTFNORM);

	ReadTitleConfig (chansSelected);
	chnConf.language ++; // umph... language in triiforce start at -1... not index friendly
	do
		{
		buff[0] = '\0';
		if (CheckParental(0))		
			{
			strcat (buff, "Set as autoboot##1|");
			
			if (chans[chansSelected].hidden)
				strcat (buff, "Remove hide flag##2|");
			else
				strcat (buff, "Hide this title ##3|");
			}

		sprintf (b, "Vote this title (%d/10)##4", chans[chansSelected].priority);
		strcat (buff, b);
		
		if (CheckParental(0))		
			{
			strcat (buff, "||");

			if (vars.neek == NEEK_NONE && config.chnBrowser.nand != NAND_REAL)
				{
				strcat (buff, "IOS: "); strcat (buff, ios[chnConf.ios]); strcat (buff, "##100|");
				}
				
			strcat (buff, "Boot method: "); strcat (buff, bootmethodoptions[chnConf.bootMode]); strcat (buff, "##106|");

			if (chnConf.bootMode < 2)
				{
				strcat (buff, "Video: "); strcat (buff, videooptions[chnConf.vmode]); strcat (buff, "##101|");
				strcat (buff, "Video Patch: "); strcat (buff, videopatchoptions[chnConf.vpatch]); strcat (buff, "##102|");
				strcat (buff, "Language: "); strcat (buff, languageoptions[chnConf.language]); strcat (buff, "##103|");
				strcat (buff, "Ocarina: "); strcat (buff, ocarinaoptions[chnConf.ocarina]); strcat (buff, "##105|");
				if (chnConf.ocarina)
					{
					strcat (buff, "Hook type: "); strcat (buff, hooktypeoptions[chnConf.hook]); strcat (buff, "##104|");
					}
				else
					strcat (buff, "Hook type: n/a##1000|");
				}
			else
				{
				strcat (buff, "Video: n/a##1000|");
				strcat (buff, "Video Patch: n/a##1000|");
				strcat (buff, "Language: n/a##1000|");
				strcat (buff, "Hook type: n/a##1000|");
				strcat (buff, "Ocarina: n/a##1000|");
				}
			}
		
		/*
		if (vars.neek != NEEK_NONE)
			{
			strcat (buff, "|Toggle UID.sys entry ##5|");
			}
			//
		*/
		
		Video_SetFontMenu(TTFSMALL);
		item = grlib_menu (300, chans[ai].name, buff);
		Video_SetFontMenu(TTFNORM);
		
		if (item < 100) break;
		if (item >= 100)
			{
			int i = item - 100;
			
			if (i == 0) { chnConf.ios ++; if (chnConf.ios >= opt[i]) chnConf.ios = 0; }
			if (i == 1) { chnConf.vmode ++; if (chnConf.vmode >= opt[i]) chnConf.vmode = 0; }
			if (i == 2) { chnConf.vpatch ++; if (chnConf.vpatch >= opt[i]) chnConf.vpatch = 0; }
			if (i == 3) { chnConf.language ++; if (chnConf.language >= opt[i]) chnConf.language = 0; }
			if (i == 4) { chnConf.hook ++; if (chnConf.hook >= opt[i]) chnConf.hook = 0; }
			if (i == 5) { chnConf.ocarina ++; if (chnConf.ocarina >= opt[i]) chnConf.ocarina = 0; }
			if (i == 6) { chnConf.bootMode ++; if (chnConf.bootMode >= opt[i]) chnConf.bootMode = 0; }
			}
		}
	while (TRUE);
	chnConf.language --;
	
	if (item == 1)
		{
		memcpy (&config.autoboot.channel, &chnConf, sizeof (s_channelConfig));
		config.autoboot.appMode = APPMODE_CHAN;
		config.autoboot.enabled = TRUE;

		config.autoboot.nand = config.chnBrowser.nand;
		strcpy (config.autoboot.nandPath, config.chnBrowser.nandPath);

		strcpy (config.autoboot.asciiId, chans[chansSelected].asciiId);
		strcpy (config.autoboot.path, chans[chansSelected].name);
		}

	if (item == 2)
		{
		chans[chansSelected].hidden = 0;
		}

	if (item == 3)
		{
		chans[chansSelected].hidden = 1;
		}

	if (item == 4)
		{
		int item;
		item = grlib_menu (0, "Vote Title", "10 (Best)|9|8|7|6|5 (Average)|4|3|2|1 (Bad)");
		if (item >= 0)
			chans[chansSelected].priority = 10-item;
		}
	/*	
	if (item == 5)
		{
		neek_UID_Remove (chans[chansSelected].titleId);
		}
	*/
	WriteTitleConfig (chansSelected);
	SortItems ();
	
	//ChnBrowse ();
	}

static void ShowFilterMenu (void)
	{
	u8 *f;
	char buff[512];
	int item;
	
	f = config.chnBrowser.filter;
	
	do
		{
		buff[0] = '\0';
		for (item = 0; item < CHANNELS_MAXFILTERS; item++)
			{
			if (item == 6) grlib_menuAddColumn (buff);
			grlib_menuAddCheckItem (buff, 100 + item, f[item], &CHANNELS_NAMES[item][1]);
			}
		
		item = grlib_menu (0, "Filter menu:\nPress (B) to close, (+) Select all, (-) Deselect all", buff);

		if (item == MNUBTN_PLUS)
			{
			int i; 	for (i = 0; i < CHANNELS_MAXFILTERS; i++) f[i] = 1;
			}

		if (item == MNUBTN_MINUS)
			{
			int i; 	for (i = 0; i < CHANNELS_MAXFILTERS; i++) f[i] = 0;
			}
		
		if (item >= 100)
			{
			int i = item - 100;
			f[i] = !f[i];
			}
		}
	while (item != -1);

	SortItems ();
	}

// Nand folder can be only root child...
#define MAXNANDFOLDERS 16
static int ScanForNandFolders (char **nf, int idx, char *device, char *nandpath)
	{
	DIR *pdir;
	struct dirent *pent;
	char path[300];
	char nand[300];

	sprintf (path, "%s://%s", device, nandpath);
	
	pdir=opendir(path);
	if (!pdir) return idx;
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if (idx == MAXNANDFOLDERS) break;
		
		sprintf (nand, "%s/%s/title/00000001", path, pent->d_name);

		if (fsop_DirExist(nand))
			{
			//grlib_dosm (nand);
			
			if (strlen(nandpath))
				sprintf (nand, "%s/%s", path, pent->d_name);
			else
				sprintf (nand, "%s%s", path, pent->d_name);

			nf[idx] = malloc (strlen(nand)+1);
			strcpy (nf[idx], nand); // Store to the list
			idx++;
			}
		}
	closedir(pdir);
	
	return idx;
	}

static void ShowNandMenu (void)
	{
	char buff[300];
	char *nandFolders[MAXNANDFOLDERS];
	int nandFodersCnt = 0;

	
	//MountDevices (true);
	Conf (0);

	buff[0] = '\0';
	
	strcat (buff, "Use Real NAND##100|");
	if (vars.neek == NEEK_NONE)
		{
		if (NandExits(DEV_SD))
			grlib_menuAddItem (buff, 101, "sd://");

		if (NandExits(DEV_USB))
			grlib_menuAddItem (buff, 102, "usb://");

		nandFodersCnt = ScanForNandFolders (nandFolders, nandFodersCnt, "sd", "");
		nandFodersCnt = ScanForNandFolders (nandFolders, nandFodersCnt, "usb", "");
		nandFodersCnt = ScanForNandFolders (nandFolders, nandFodersCnt, "usb", "nands");

		int i, id = 200;
		for (i = 0;  i < nandFodersCnt; i++)
			grlib_menuAddItem (buff, id++, nandFolders[i]);
		}
		
	strcat (buff, "|");
	strcat (buff, "Cancel##-1");
		
	Redraw();
	grlib_PushScreen();
	
	int item = grlib_menu (0, "Select NAND Source", buff);

	if (item > 0)
		config.chnBrowser.nandPath[0] = '\0';

	if (item == 100)
		{
		config.chnBrowser.nand = NAND_REAL;
		browse = 1;
		}

	if (item == 101)
		{
		config.chnBrowser.nand = NAND_EMUSD;
		browse = 1;
		}

	if (item == 102)
		{
		config.chnBrowser.nand = NAND_EMUUSB;
		browse = 1;
		}
	
	if (item >= 200)
		{
		int i = item - 200;
		char dev[10];
		
		strcpy (dev, "sd://");
		if (strstr (nandFolders[i], dev))
			{
			config.chnBrowser.nand = NAND_EMUSD;
			strcpy (config.chnBrowser.nandPath, &nandFolders[i][strlen(dev)-1]);
			}
			
		strcpy (dev, "usb://");
		if (strstr (nandFolders[i], dev))
			{
			config.chnBrowser.nand = NAND_EMUUSB;
			strcpy (config.chnBrowser.nandPath, &nandFolders[i][strlen(dev)-1]);
			}
		
		//grlib_dosm ("%d, %s", config.chnBrowser.nand, config.chnBrowser.nandPath);	
		browse = 1;
		}
	
	int i; for (i = 0; i < nandFodersCnt; i++) free (nandFolders[i]);
	
	Conf (1);
	}

static void SyncNandFile (char *sourcepath, char *sourcefn)
	{
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char target[256];
	u8 *buffer;
	s32 err;
	size_t readed;
	
	Video_WaitPanel (TEX_HGL, "Please wait...|Copying SYSCONF");

	//ISFS init
	err = ISFS_Initialize();
	sprintf (path, "%s/%s", sourcepath, sourcefn);
	buffer = isfs_ReadFile (path, &err, 0, &readed);
	ISFS_Deinitialize();

	Debug ("SyncNandFile: '%s'->0x%X, %u bytes, [err = %d]", path, buffer, readed, err);

	if (!buffer)
		{
		// Add message here
		grlib_menu (0, "Cannot open source file!", " OK ");
		return;
		}
		
	if (config.chnBrowser.nand == NAND_EMUSD)
		sprintf (target, "sd:/%s%s", config.chnBrowser.nandPath,sourcepath);
	else
		sprintf (target, "usb:/%s%s", config.chnBrowser.nandPath,sourcepath);
		
	fsop_CreateFolderTree (target);
	strcat (target, "/");
	strcat (target, sourcefn);

	Debug ("SyncNandFile: target is '%s'", target);
	if (fsop_WriteFile (target, buffer, readed))
		{
		grlib_menu (0, "Operation completed succesfully!", " OK ");
		}
		
	free (buffer);
	}
	
static void ChangeDefaultIOS (void)
	{
	char buff[1024];
	
	buff[0] = '\0';
	
	// "CFG", "GX", "WiiFlow"
	
	grlib_menuAddItem (buff, 4, "247");
	grlib_menuAddItem (buff, 5, "248");
	grlib_menuAddItem (buff, 0, "249");
	grlib_menuAddItem (buff, 1, "250");
	grlib_menuAddItem (buff, 2, "251");
	grlib_menuAddItem (buff, 3, "252");
	
	int item = grlib_menu (0, "Select your default CIOS slot", buff);
	if (item < 0) return;
		
	Redraw();
	grlib_PushScreen();

	int i;
	int j = 0;
	for (i = 0; i < chansCnt; i++)
		{
		ReadTitleConfig (i);
		chnConf.ios = item;
		WriteTitleConfig (i);
			
		if (j++ > 10)
			{
			Video_WaitPanel (TEX_HGL, "Updating configuration files|%d%%", (i * 100)/chansCnt);
			j = 0;
			}
		}
	}
	
static void ShowMainMenu (void)
	{
	char buff[512];
	
	buff[0] = '\0';
	
	if (CheckParental(0))
		{
		grlib_menuAddItem (buff,  1, "NAND Source...");
		grlib_menuAddItem (buff,  2, "Refresh cache...");
		grlib_menuAddItem (buff,  3, "Download covers...");
		}
		
	grlib_menuAddItem (buff,  4, "Titles filter");

	if (CheckParental(0))
		{
		if (config.chnBrowser.nand != NAND_REAL)
			{
			grlib_menuAddItem (buff,  5, "Copy SYSCONF from real nand...");
			grlib_menuAddItem (buff,  6, "Copy MII from real nand...");
			}

		if (showHidden)
			grlib_menuAddItem (buff,  7, "Hide hidden titles");
		else
			grlib_menuAddItem (buff,  8, "Show hidden titles");

		if (vars.neek != NEEK_NONE)
			{
			grlib_menuAddItem (buff,  9, "Manage UID.sys");
			}
		else
			{
			grlib_menuAddItem (buff,  10, "Change Default CIOSX");
			}
		}
		
	Redraw();
	grlib_PushScreen();
	
	int item = grlib_menu (0, "Channel menu", buff);
			
	if (item == 1)
		{
		ShowNandMenu();
		}
	
	if (item == 2)
		{
		RebuildCacheFile ();
		ChnBrowse ();
		}

	if (item == 3)
		{
		DownloadCovers();
		}

	if (item == 4)
		{
		ShowFilterMenu ();
		}
		
	if (item == 5)
		{
		SyncNandFile ("/shared2/sys", "SYSCONF");
		}
		
	if (item == 6)
		{
		SyncNandFile ("/shared2/menu/FaceLib", "RFL_DB.dat");
		}
		
	if (item == 7)
		{
		showHidden = 0;
		}

	if (item == 8)
		{
		showHidden = 1;
		}
		
	if (item == 9)
		{
		manageUID = 1;
		grlib_menu (0, "Manage UID.sys\n\nPress (A) to hide/show title\nPress (B) to exit from editing mode\nPress (1) add/remove item from uid.sys\nPress (2) to show/hide ALLL entries from uid.sys", "  OK  ");
		}

	if (item == 10)
		{
		ChangeDefaultIOS ();
		}

	SortItems ();
	}

static void CheckFilters()
	{
	int i, j;
	
	j = 0;
	for (i = 0; i < CHANNELS_MAXFILTERS; i++)
		if (config.chnBrowser.filter[i]) j++;
		
	if (j == 0)
		for (i = 0; i < CHANNELS_MAXFILTERS; i++)
		config.chnBrowser.filter[i] = 1;
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
		
		if (ai < chansCnt && ai < chans2Disp && gui.spotsIdx < SPOTSMAX)
			{
			// Draw application png
			sprintf (path, "%s://ploader/covers/%s.png", vars.defMount, chans[ai].asciiId);
			gui.spots[gui.spotsIdx].ico.icon = CoverCache_Get(path);
			
			// Need a name ?	
			if (!gui.spots[gui.spotsIdx].ico.icon)
				{
				char title[256];
				strcpy (title, chans[ai].name);
				title[48] = 0;
				strcpy (gui.spots[gui.spotsIdx].ico.title, title);
				}
			else
				gui.spots[gui.spotsIdx].ico.title[0] = '\0';

			// Is it hidden ?
			if (showHidden && chans[ai].hidden)
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = vars.tex[TEX_GHOST];
			else
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = NULL;
				
			if (manageUID && neek_UID_Find (chans[ai].titleId) >= 0)
				gui.spots[gui.spotsIdx].ico.iconOverlay[2] = vars.tex[TEX_CHECKED];
			else
				gui.spots[gui.spotsIdx].ico.iconOverlay[2] = NULL;

			if (manageUID)
				{
				if (neek_UID_IsHidden (chans[ai].titleId))
					{
					Debug ("Hidden item");
					gui.spots[gui.spotsIdx].ico.iconOverlay[1] = vars.tex[TEX_GHOST];
					}
				else
					gui.spots[gui.spotsIdx].ico.iconOverlay[1] = NULL;
				}

			grlib_IconDraw (&is, &gui.spots[gui.spotsIdx].ico);

			// Let's add the spot points, to reconize the icon pointed by wiimote
			gui.spots[gui.spotsIdx].id = ai;
			
			gui.spotsIdx++;
			}
		else
			{
			s_grlib_icon ico;
			grlib_IconInit (&ico, &gui.spots[i].ico);
			
			ico.iconOverlay[0] = NULL;
			ico.iconOverlay[1] = NULL;
			ico.iconOverlay[2] = NULL;
			ico.iconOverlay[3] = NULL;

			ico.noIcon = true;
			grlib_IconDraw (&is, &ico);
			}
		}
	}

static void Redraw (void)
	{
	char title[128];
	
	Video_DrawBackgroud (1);
	
	if (config.chnBrowser.nand == NAND_REAL)
		{
		if (vars.neek == NEEK_NONE)
			strcpy (title, "postLoader::Channels (real nand)");
		else
			strcpy (title, "postLoader::Channels (neek nand)");
		}
	else if (config.chnBrowser.nand == NAND_EMUSD)
		sprintf (title, "postLoader::Channels (nand sd:/%s)", config.chnBrowser.nandPath);
	else if (config.chnBrowser.nand == NAND_EMUUSB)
		sprintf (title, "postLoader::Channels (nand usb:/%s)", config.chnBrowser.nandPath);
		
	if (manageUID)
		{
		sprintf (title, "postLoader::Managing uid.sys, %d items (%d hidden)", neek_UID_Count (), neek_UID_CountHidden ());
		}
	
	Video_SetFont(TTFNORM);
	grlib_Text ( 25, 26, GRLIB_ALIGNLEFT, 0, title);
	grlib_printf ( 615, 26, GRLIB_ALIGNRIGHT, 0, "Page %d of %d", page+1, pageMax+1);
	
	if (redrawIcons) RedrawIcons (0,0);
	
	Video_DrawVersionInfo ();
	
	if (chansCnt == 0 && nandScanned == 1)
		{
		Video_SetFont(TTFNORM);
		grlib_DrawCenteredWindow ("No titles found, rebuild cache!", WAITPANWIDTH, 133, 0, NULL);
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
	
static bool QuerySelection (int ai)
	{
	int i;
	float mag = 1.0;
	int spot = -1;
	int incX = 1, incY = 1;
	int y = 220;
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

		DrawInfoWindo (yInf, chans[ai].name, "Press (A) to start, (B) Cancel");
		yInf -= 5;
		if (yInf < 400) yInf = 400;

		Overlay ();
		grlib_DrawIRCursor ();
		grlib_Render();
		
		if (mag < 2.7) mag += 0.05;
		if (mag >= 2.7 && ico.x == 320 && ico.y == y) break;
		}
	
	vars.useChannelCompatibleMode = 0;	
	u32 btn;
	while (true)
		{
		grlib_PopScreen ();
		grlib_DrawSquare (&black);
		grlib_IconDraw (&istemp, &ico);
		Overlay ();
		
		DrawInfoWindo (yInf, chans[ai].name, "Press (A) to start, (B) Cancel");
		yInf -= 5;
		if (yInf < 400) yInf = 400;
		
		grlib_DrawIRCursor ();
		grlib_Render();

		btn = grlib_GetUserInput();
		if (btn & WPAD_BUTTON_A) return true;
		if (btn & WPAD_BUTTON_PLUS) {vars.useChannelCompatibleMode = 1; return true;}
		if (btn & WPAD_BUTTON_B) return false;
		}
	return true;
	}

int ChnBrowser (void)
	{
	u32 btn;
	
	Debug ("ChnBrowser (begin)");
	
	Conf (true);
	
	if (vars.neek != NEEK_NONE)
		config.chnBrowser.nand = NAND_REAL;

	browserRet = -1;
	nandScanned = false;
	
	grlibSettings.color_window = RGBA(192,192,192,255);
	grlibSettings.color_windowBg = RGBA(32,32,32,128);
	
	grlib_SetRedrawCallback (Redraw, Overlay);
	
	chans = calloc (CHNMAX, sizeof(s_channel));
	
	// Immediately draw the screen...
	ChansFree ();
	InitializeGui ();
	
	/*
	Redraw ();
	grlib_PushScreen ();
	grlib_PopScreen ();
	grlib_Render();  // Render the theme.frame buffer to the TV
	*/
	
	CheckFilters ();
	
	page = config.chnPage;
	ChnBrowse ();
	
	redraw = 1;
	
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
			
			if (btn & WPAD_BUTTON_1 && !manageUID) 
				{
				page = GoToPage (page, pageMax);
				FeedCoverCache ();
				redraw = 1;
				}
			
			if (btn & WPAD_BUTTON_A && chansSelected != -1 && !manageUID)
				{
				ReadTitleConfig (chansSelected);
				if (!QuerySelection(chansSelected))
					{
					redraw = 1;
					continue;
					}
					
				memcpy (&config.run.channel, &chnConf, sizeof(s_channelConfig));
				config.run.nand = config.chnBrowser.nand;
				strcpy (config.run.nandPath, config.chnBrowser.nandPath);
				
				browserRet = INTERACTIVE_RET_CHANSEL;
				}

			if (btn & WPAD_BUTTON_A && chansSelected != -1 && manageUID)
				{
				int ret = neek_UID_Hide(chans[chansSelected].titleId);
				
				if (!ret)
					neek_UID_Show(chans[chansSelected].titleId);
				
				redraw = 1;
				btn = 0;
				}
				
			if (btn & WPAD_BUTTON_1 && chansSelected != -1 && manageUID)
				{
				int ret = neek_UID_Remove(chans[chansSelected].titleId);
				
				if (!ret)
					neek_UID_Add(chans[chansSelected].titleId);
				
				redraw = 1;
				btn = 0;
				}
				
			if (btn & WPAD_BUTTON_B && manageUID)
				{
				manageUID = 0;
				int ret = grlib_menu (0, "Manage UID.sys\n\nOperation completed. Do you want to save the new UID.sys ?", "  YES  ##1~  NO  ##0");
				if (ret == 1)
					{
					neek_UID_Write ();
					}
				redraw = 1;
				btn = 0;
				}
				
			if (btn & WPAD_BUTTON_2 && manageUID)
				{
				if (neek_UID_Count() == 0)
					{
					int i;
					for (i = 0; i < chansCnt; i++)
						{
						neek_UID_Add (chans[i].titleId);
						}
					}
				else
					neek_UID_Clean (); // return the number of erased items
					
				redraw = 1;
				btn = 0;
				}
				
			if (btn & WPAD_BUTTON_B && chansSelected != -1 && !manageUID)
				{
				ShowAppMenu (chansSelected);
				redraw = 1;
				btn = 0;
				}

			if (btn & WPAD_BUTTON_2 && !manageUID)
				{
				ShowFilterMenu ();
				redraw = 1;
				btn = 0;
				}
				
			if (btn & WPAD_BUTTON_HOME)
				{
				ShowMainMenu ();
				redraw = 1;
				btn = 0;
				}
			
			if (btn & WPAD_BUTTON_MINUS)
				{
				page = ChangePage (0);
				btn = 0;
				}
				
			if (btn & WPAD_BUTTON_PLUS) 
				{
				page = ChangePage (1);
				btn = 0;
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
			ChnBrowse ();
			browse = 0;
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
			browserRet = INTERACTIVE_RET_TOCHANNELS;
			}
		}
		
	// Lets close the topbar, if needed
	CLOSETOPBAR();
	CLOSEBOTTOMBAR();

	// save current page
	config.chnPage = page;

	Conf (false);
	
	// Clean up all data
	ChansFree ();
	gui_Clean ();
	free (chans);
	
	grlib_SetRedrawCallback (NULL, NULL);
	
	Debug ("ChnBrowser (end)");
	
	return browserRet;
	}