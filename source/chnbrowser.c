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

static u8 redraw = 1;
static bool redrawIcons = true;

static s_grlib_iconSetting is;

static void Redraw (void);
static int ChnBrowse (void);

static bool nandScanned = false;

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
		
		if (ai >= 0 && ai < chansCnt)
			{
			sprintf (path, "%s://ploader/covers/%s.png", vars.defMount, chans[ai].asciiId);
			CoverCache_Add (path, false);
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
					sprintf (buff, "http://art.gametdb.com/wii/wwtitle/%s/%s.png", regions[ireg], chans[ia].asciiId);
					ret = DownloadCovers_Get (path, buff);
					}
				if (!ret)
					{
					sprintf (buff, "http://art.gametdb.com/wii/wwtitlealt/%s/%s.png", regions[ireg], chans[ia].asciiId);
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
	
	ChnBrowse ();
	FeedCoverCache ();
	}
	
static void WriteTitleConfig (int ia)
	{
	if (ia < 0) return;
	
	chnConf.titleId = chans[ia].titleId;
	chnConf.hidden = chans[ia].hidden;
	chnConf.priority = chans[ia].priority;
	
	char *buff = Bin2HexAscii (&chnConf, sizeof (s_channelConfig), 0);
	cfg_SetString (cfg, chans[ia].asciiId, buff);
	free (buff);
	}

static void ReadTitleConfig (int ia)
	{
	char buff[1024];
	bool valid;
	
	valid = cfg_GetString (cfg, chans[ia].asciiId, buff);
	
	if (valid)
		{
		if (HexAscii2Bin (buff, &chnConf) != sizeof (s_channelConfig))
			{
			Debug ("ReadTitleConfig: Error -> size fails");
			valid = false;
			}
		}
	
	if (!valid)
		{
		chnConf.priority = 5;
		chnConf.hidden = 0;
		chnConf.titleId = chans[ia].titleId;
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
		}

	chans[ia].titleId = chnConf.titleId;
	chans[ia].hidden = chnConf.hidden;
	chans[ia].priority = chnConf.priority;
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
	
static void SaveTitlesCache (void)
	{
	FILE* f = NULL;
	char cfg[256];
	char buff[256];
	int i;
	
	sprintf (cfg, "%s://ploader/channels.txt", vars.defMount);
	f = fopen(cfg, "wb");
	if (!f) return;
	
	for (i = 0; i < chansCnt; i++)
		{
		sprintf (buff, "%s:%s\n", chans[i].asciiId, chans[i].name);
		fwrite (buff, 1, strlen(buff), f );
		}
	fclose(f);
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

#define SKIP 10
static void AppsSort (void)
	{
	int i;
	int mooved;
	s_channel app;
	
	// Apply filters
	chans2Disp = 0;
	for (i = 0; i < chansCnt; i++)
		{
		chans[i].filterd = CheckFilter(i);
		if (chans[i].filterd && (!chans[i].hidden || showHidden)) chans2Disp++;
		}
	
	// Sort by filter, use stupid algorithm...
	do
		{
		mooved = 0;
		
		for (i = 0; i < chansCnt - 1; i++)
			{
			if (chans[i].filterd < chans[i+1].filterd)
				{
				// swap
				memcpy (&app, &chans[i+1], sizeof(s_channel));
				memcpy (&chans[i+1], &chans[i], sizeof(s_channel));
				memcpy (&chans[i], &app, sizeof(s_channel));
				mooved = 1;
				}
			}
		}
	while (mooved);

	// Sort by hidden, use stupid algorithm...
	do
		{
		mooved = 0;
		
		for (i = 0; i < chansCnt - 1; i++)
			{
			if (chans[i].hidden > chans[i+1].hidden)
				{
				// swap
				memcpy (&app, &chans[i+1], sizeof(s_channel));
				memcpy (&chans[i+1], &chans[i], sizeof(s_channel));
				memcpy (&chans[i], &app, sizeof(s_channel));
				mooved = 1;
				}
			}
		}
	while (mooved);

	// Sort by name, use stupid algorithm...
	do
		{
		mooved = 0;
		
		for (i = 0; i < chans2Disp - 1; i++)
			{
			if (chans[i].name && chans[i+1].name && ms_strcmp (chans[i+1].name, chans[i].name) < 0)
				{
				// swap
				memcpy (&app, &chans[i+1], sizeof(s_channel));
				memcpy (&chans[i+1], &chans[i], sizeof(s_channel));
				memcpy (&chans[i], &app, sizeof(s_channel));
				mooved = 1;
				}
			}
		}
	while (mooved);

	// Sort by priority
	do
		{
		mooved = 0;
		
		for (i = 0; i < chans2Disp - 1; i++)
			{
			if (chans[i+1].priority > chans[i].priority)
				{
				// swap
				memcpy (&app, &chans[i+1], sizeof(s_channel));
				memcpy (&chans[i+1], &chans[i], sizeof(s_channel));
				memcpy (&chans[i], &app, sizeof(s_channel));
				mooved = 1;
				}
			}
		}
	while (mooved);

	pageMax = (chans2Disp-1) / gui.spotsXpage;
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
				grlib_menu ("postLoader was unable to patch ISFS permission. Cannot continue", "  OK  ");
				return -1;
				}
			}
		else if (vars.ios == IOS_CIOS)
			{
			Video_WaitPanel (TEX_CHIP, "Please wait...");
			}
		else
			{
			grlib_menu ("Unsupported ios.\npostLoader need 58+AHPBROT or cIOSX rev21d2x on slot 249.\nCannot continue", "  OK  ");
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

	cnt = 0;
	for (id = 0; id < 2; id++)
		{
		if (*path == 0)
			ret = get_game_list(&TitleIds, &Titlecount, id);
		else
			ret = get_game_listEmu(&TitleIds, &Titlecount, id, path);
		
		if (ret > 0)
			{
			for (i = 0; i < Titlecount; i++)
				{
				lower[cnt] = TITLE_LOWER(TitleIds[i]);
				upper[cnt] = TITLE_UPPER(TitleIds[i]);
				
				// Search title in cache
				names[cnt] = get_name(TitleIds[i], path);
				
				Video_WaitPanel (TEX_HGL, "Searching for channels: %d found...", cnt);
				Debug ("RebuildCacheFile: found '%s'", names[cnt]);
				
				cnt++;
				}
			
			free (TitleIds);
			}
		}

	if (config.chnBrowser.nand == NAND_REAL) ISFS_Deinitialize();

	GetCacheFileName (path);
	
	Debug ("RebuildCacheFile: storing cache '%s'", path);
	
	FILE* f = NULL;
	char buff[256];
	
	f = fopen(path, "wb");
	if (!f) return 0;
	
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

static void UpdateTitlesFromTxt (void)
	{
	LoadTitlesTxt ();
	if (titlestxt == NULL) return;
	
	int i;
	char buff[1024];
	for (i = 0; i < chansCnt; i++)
		{
		if (cfg_GetString (titlestxt, chans[i].asciiId, buff))
			{
			Debug ("UpdateTitlesFromTxt: '%s' -> '%s'", chans[i].name, buff);
			free (chans[i].name);
			chans[i].name = ms_utf8_to_ascii (buff);
			}
		
		if (i % 20 == 0) Video_WaitPanel (TEX_HGL, "Please wait...|Parsing...");
		}
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
	
	titles = (char*)ReadFile2Buffer (path, &size, NULL, TRUE);
	if (!titles)
		{
		Debug ("ChnBrowse: ReadFile2Buffer failed '%s'", path);
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
		for (i = 0; i < 63; i++)
			{
			if (*p == '\n' || *p == '\0') 
				break;
			else
				name[i] = *p;
			
			p++;
			}
		p++;
		
		//grlib_dosm ("%d %d %d: %s", size, chansCnt, p - titles, name);
			
		if (upper > 0 && lower > 0 && strlen (name) >= 4)
			{
			chans[chansCnt].titleId = TITLE_ID (upper, lower);
			memset(chans[chansCnt].asciiId, 0, 6);
			memcpy(chans[chansCnt].asciiId, (char *)(&lower), 4);

			// Search title in cache
			chans[chansCnt].name = malloc (strlen(name)+1);
			if (chans[chansCnt].name)
				strcpy (chans[chansCnt].name, name);

			// Update configuration
			ReadTitleConfig (chansCnt);
			
			if (chansCnt % 20 == 0) Video_WaitPanel (TEX_HGL, "Please wait...|Loading channels configuration");

			chans[chansCnt].png = NULL;
			chansCnt++;
			}
		}
		
	/*
	if (vars.neek != NEEK_NONE)
		neek_UID_Dump ();
	*/
	UpdateTitlesFromTxt ();
	AppsSort ();
	
	nandScanned = true;
	
	free (titles);
	
	Debug ("ChnBrowse: done!");
	return chansCnt;
	}

static int FindSpot (void)
	{
	int i,j;
	static time_t t = 0;
	char info[300];
	
	chansSelected = -1;
	
	for (i = 0; i < gui.spotsIdx; i++)
		{
		if (grlib_irPos.x > gui.spots[i].ico.rx1 && grlib_irPos.x < gui.spots[i].ico.rx2 && grlib_irPos.y > gui.spots[i].ico.ry1 && grlib_irPos.y < gui.spots[i].ico.ry2)
			{
			// Ok, we have the point
			chansSelected = gui.spots[i].id;

			gui.spots[i].ico.sel = true;
			grlib_IconDraw (&is, &gui.spots[i].ico);

			grlib_SetFontBMF (fonts[FNTNORM]);
			grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, chans[chansSelected].name);
			
			grlib_SetFontBMF (fonts[FNTSMALL]);
			
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
	
	grlib_SetFontBMF (fonts[FNTNORM]);
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
	
	grlib_SetFontBMF(fonts[FNTNORM]);

	ReadTitleConfig (chansSelected);
	chnConf.language ++; // umph... language in triiforce start at -1... not index friendly
	do
		{
		buff[0] = '\0';
		strcat (buff, "Set as autoboot##1|");
		
		if (chans[chansSelected].hidden)
			strcat (buff, "Remove hide flag##2|");
		else
			strcat (buff, "Hide this title ##3|");

		sprintf (b, "Vote this title (%d/10)##4", chans[chansSelected].priority);
		strcat (buff, b);
		
		//if (config.chnBrowser.nand != NAND_REAL)
		
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
			strcat (buff, "Hook type: "); strcat (buff, hooktypeoptions[chnConf.hook]); strcat (buff, "##104|");
			strcat (buff, "Ocarina: "); strcat (buff, ocarinaoptions[chnConf.ocarina]); strcat (buff, "##105|");
			}
		else
			{
			strcat (buff, "Video: n/a##1000|");
			strcat (buff, "Video Patch: n/a##1000|");
			strcat (buff, "Language: n/a##1000|");
			strcat (buff, "Hook type: n/a##1000|");
			strcat (buff, "Ocarina: n/a##1000|");
			}
		
		grlibSettings.fontNormBMF = fonts[FNTBIG];
		item = grlib_menu (chans[ai].name, buff);
		grlibSettings.fontNormBMF = fonts[FNTNORM];
		
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
		WriteTitleConfig (chansSelected);
		AppsSort ();
		}

	if (item == 3)
		{
		chans[chansSelected].hidden = 1;
		WriteTitleConfig (chansSelected);
		AppsSort ();
		}

	if (item == 4)
		{
		int item;
		item = grlib_menu ("Vote Title", "10 (Best)|9|8|7|6|5 (Average)|4|3|2|1 (Bad)");
		if (item >= 0)
			chans[chansSelected].priority = 10-item;
		
		WriteTitleConfig (chansSelected);
		AppsSort ();
		}

	WriteTitleConfig (chansSelected);
	
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
		
		item = grlib_menu ("Filter menu:\nPress (B) to close, (+) Select all, (-) Deselect all", buff);

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
	ChnBrowse ();
	AppsSort ();
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
	
	grlibSettings.fontNormBMF = fonts[FNTBIG];
	int item = grlib_menu ("Select NAND Source", buff);
	grlibSettings.fontNormBMF = fonts[FNTNORM];

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
	}

	
static void ShowNandOptions (void)
	{
	char buff[300];
	
	//MountDevices (true);
	
	buff[0] = '\0';
	
	strcat (buff, "Update title cache...##9|");
	strcat (buff, "Download covers...##10||");
	strcat (buff, "Cancel##-1");
		
	Redraw();
	grlib_PushScreen();
	
	grlibSettings.fontNormBMF = fonts[FNTBIG];
	int item = grlib_menu ("NAND Options", buff);
	grlibSettings.fontNormBMF = fonts[FNTNORM];
		
	if (item == 9)
		{
		RebuildCacheFile ();
		ChnBrowse ();
		FeedCoverCache ();
		}

	if (item == 10)
		{
		DownloadCovers();
		}
	}

	
static void ShowMainMenu (void)
	{
	char buff[512];
	
	buff[0] = '\0';
	
	grlib_menuAddItem (buff, 100, "Browse to...");
	grlib_menuAddItem (buff,  1, "NAND Source...");
	grlib_menuAddItem (buff,  8, "NAND Options...");
	grlib_menuAddItem (buff,  3, "Titles filter");

	if (showHidden)
		grlib_menuAddItem (buff,  6, "Hide hidden titles");
	else
		grlib_menuAddItem (buff,  7, "Show hidden titles");

	grlib_menuAddColumn (buff);
	grlib_menuAddItem (buff,  4, "Run System menu");
	grlib_menuAddItem (buff, 20, "Run BOOTMII");
	grlib_menuAddItem (buff, 21, "Run DISC");
	grlib_menuAddItem (buff,  5, "System options...");
	grlib_menuAddItem (buff, -1, "Close");
		
	Redraw();
	grlib_PushScreen();
	
	grlibSettings.fontNormBMF = fonts[FNTBIG];
	int item = grlib_menu ("Channel menu", buff);
	grlibSettings.fontNormBMF = fonts[FNTNORM];
		
	if (item == 100)
		{
		browserRet = Menu_SelectBrowsingMode ();
		}
		
	if (item == 3)
		{
		ShowFilterMenu ();
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
		showHidden = 0;
		AppsSort ();
		}

	if (item == 7)
		{
		showHidden = 1;
		AppsSort ();
		}
		
	if (item == 20)
		{
		browserRet = INTERACTIVE_RET_BOOTMII;
		}

	if (item == 21)
		{
		browserRet = INTERACTIVE_RET_DISC;
		}

	if (item == 1)
		{
		ShowNandMenu();
		}
	
	if (item == 8)
		{
		ShowNandOptions();
		}
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
				strcpy (gui.spots[gui.spotsIdx].ico.title, chans[ai].name);
			else
				gui.spots[gui.spotsIdx].ico.title[0] = '\0';

			// Is it hidden ?
			if (chans[ai].hidden && showHidden)
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = vars.tex[TEX_GHOST];
			else
				gui.spots[gui.spotsIdx].ico.iconOverlay[1] = NULL;

			// Is it hidden ?
			if (chans[ai].hidden && showHidden)
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
	}

static void Redraw (void)
	{
	char sdev[64];
	
	Video_DrawBackgroud (1);
	
	if (config.chnBrowser.nand == NAND_REAL)
		strcpy (sdev, "Real NAND");
	else if (config.chnBrowser.nand == NAND_EMUSD)
		sprintf (sdev, "EmuNAND [SD] %s", config.chnBrowser.nandPath);
	else if (config.chnBrowser.nand == NAND_EMUUSB)
		sprintf (sdev, "EmuNAND [USB] %s", config.chnBrowser.nandPath);
	
	grlib_SetFontBMF(fonts[FNTNORM]);
	char ahpbrot[16];
	if (vars.ahbprot)
		strcpy (ahpbrot,"+");
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
			grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader"VER" (%s %s) - NAND", neek, vars.neekName);
		else
			grlib_printf ( 25, 26, GRLIB_ALIGNLEFT, 0, "postLoader"VER" (%s) - NAND", neek);
		}
		
	grlib_printf ( 615, 26, GRLIB_ALIGNRIGHT, 0, "Page %d of %d", page+1, pageMax+1);
	
	if (redrawIcons) RedrawIcons (0,0);
	
	grlib_SetFontBMF(fonts[FNTNORM]);
	
	if (chansCnt == 0 && nandScanned == 1)
		{
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
		{
		if (page == pageMax) return page;
		page++;
		}
	else
		{
		if (page == 0) return page;
		page--;
		}
		
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
			page = lp+1;
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
			page = lp-1;
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
		
		if (mag < 2.7) mag += 0.05;
		if (mag >= 2.7 && ico.x == 320 && ico.y == y) break;
		}
	
	vars.useChannelCompatibleMode = 0;	
	int fr = grlibSettings.fontBMF_reverse;
	u32 btn;
	while (true)
		{
		grlib_PopScreen ();
		grlib_DrawSquare (&black);
		grlib_IconDraw (&istemp, &ico);
		Overlay ();
		
		grlibSettings.fontBMF_reverse = 0;
		grlib_printf (XMIDLEINFO, theme.line1Y, GRLIB_ALIGNCENTER, 0, chans[ai].name);
		grlib_printf (XMIDLEINFO, theme.line2Y, GRLIB_ALIGNCENTER, 0, "Press (A) to start, (B) Cancel");		

		grlibSettings.fontBMF_reverse = fr;
		
		grlib_GetUserInput();
		grlib_DrawIRCursor ();
		grlib_Render();

		btn = grlib_GetUserInput();
		if (btn & WPAD_BUTTON_A) return true;
		if (btn & WPAD_BUTTON_PLUS) {vars.useChannelCompatibleMode = 1; return true;}
		if (btn & WPAD_BUTTON_B) return false;
		}
	return true;
	}

static void Conf (bool open)
	{
	char cfgpath[64];
	
	if (vars.neek != NEEK_NONE)
		sprintf (cfgpath, "%s://ploader/channels.neek", vars.defMount);
	else if (config.chnBrowser.nand == NAND_REAL)
		sprintf (cfgpath, "%s://ploader/channels.real", vars.defMount);
	else
		sprintf (cfgpath, "%s://ploader/channels.emul", vars.defMount);

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
	
	Redraw ();
	grlib_PushScreen ();
	grlib_PopScreen ();
	grlib_Render();  // Render the theme.frame buffer to the TV
	
	CheckFilters ();
	ChnBrowse ();
	
	redraw = 1;
	
	if (config.chnPage >= 0 && config.chnPage <= pageMax)
		page = config.chnPage;
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
				page = GoToPage (page, pageMax);
				FeedCoverCache ();
				redraw = 1;
				}
			
			if (btn & WPAD_BUTTON_A && chansSelected != -1) 
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
				break;
				}
				
			/////////////////////////////////////////////////////////////////////////////////////////////
			// Select application as default
			if (btn & WPAD_BUTTON_B && chansSelected != -1)
				{
				ShowAppMenu (chansSelected);
				redraw = 1;
				}

			if (btn & WPAD_BUTTON_2)
				{
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
		
		grlib_PopScreen ();
		FindSpot ();
		Overlay ();
		grlib_DrawIRCursor ();
        grlib_Render();  // Render the theme.frame buffer to the TV
		
		if (browse)
			{
			ChnBrowse ();
			FeedCoverCache ();
			browse = 0;
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
			browserRet = INTERACTIVE_RET_TOCHANNELS;
			}

		usleep (5000);
		}

	// save current page
	config.chnPage = page;

	SaveTitlesCache ();

	Conf (false);
	
	// Clean up all data
	ChansFree ();
	gui_Clean ();
	free (chans);
	
	grlib_SetRedrawCallback (NULL, NULL);
	
	Debug ("ChnBrowser (end)");
	
	return browserRet;
	}