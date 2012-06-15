#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/video_types.h>
#include <dirent.h>
#include "globals.h"
#include "fsop/fsop.h"
#include "devices.h"
#include "mystring.h"

#define DMLVER "DMLSDAT0001"
#define SEP 0xFF

#define BC 0x0000000100000100ULL
#define MIOS 0x0000000100000101ULL

/** Base address for video registers. */
#define MEM_VIDEO_BASE (0xCC002000)
#define IOCTL_DI_DVDLowAudioBufferConfig 0xE4

#define PLGC_Auto 0
#define PLGC_Game 1
#define PLGC_WII 2
#define PLGC_NTSC 3
#define PLGC_PAL50 4
#define PLGC_PAL60 5

#define VIDEO_MODE_NTSC 0
#define VIDEO_MODE_PAL 1
#define VIDEO_MODE_PAL60 2
#define VIDEO_MODE_NTSC480P 3
#define VIDEO_MODE_PAL480P 4

#define SRAM_ENGLISH 0
#define SRAM_GERMAN 1
#define SRAM_FRENCH 2
#define SRAM_SPANISH 3
#define SRAM_ITALIAN 4
#define SRAM_DUTCH 5

typedef struct DML_CFG
{
        u32 Magicbytes;                 //0xD1050CF6
        u32 CfgVersion;                 //0x00000001
        u32 VideoMode;
        u32 Config;
        char GamePath[255];
        char CheatPath[255];
} DML_CFG;

enum dmlconfig
{
        DML_CFG_CHEATS          = (1<<0),
        DML_CFG_DEBUGGER        = (1<<1),
        DML_CFG_DEBUGWAIT       = (1<<2),
        DML_CFG_NMM             = (1<<3),
        DML_CFG_NMM_DEBUG       = (1<<4),
        DML_CFG_GAME_PATH       = (1<<5),
        DML_CFG_CHEAT_PATH      = (1<<6),
        DML_CFG_ACTIVITY_LED	= (1<<7),
        DML_CFG_PADHOOK         = (1<<8),
        DML_CFG_NODISC          = (1<<9),
        DML_CFG_BOOT_DISC       = (1<<10),
        DML_CFG_BOOT_DOL        = (1<<11),
};

enum dmlvideomode
{
        DML_VID_DML_AUTO        = (0<<16),
        DML_VID_FORCE           = (1<<16),
        DML_VID_NONE            = (2<<16),

        DML_VID_FORCE_PAL50     = (1<<0),
        DML_VID_FORCE_PAL60     = (1<<1),
        DML_VID_FORCE_NTSC      = (1<<2),
        DML_VID_FORCE_PROG      = (1<<3),
        DML_VID_PROG_PATCH      = (1<<4),
};





static char *dmlFolders[] = {"ngc", "games"};

syssram* __SYS_LockSram();
u32 __SYS_UnlockSram(u32 write);
u32 __SYS_SyncSram(void);

s32 setstreaming()
	{
	char __di_fs[] ATTRIBUTE_ALIGN(32) = "/dev/di";
	u32 bufferin[0x20] __attribute__((aligned(32)));
	u32 bufferout[0x20] __attribute__((aligned(32)));
	s32 __dvd_fd = -1;
	
	u8 ioctl;
	ioctl = IOCTL_DI_DVDLowAudioBufferConfig;

	__dvd_fd = IOS_Open(__di_fs,0);
	if(__dvd_fd < 0) return __dvd_fd;

	memset(bufferin, 0, 0x20);
	memset(bufferout, 0, 0x20);

	bufferin[0] = (ioctl << 24);

	if ( (*(u32*)0x80000008)>>24 )
		{
		bufferin[1] = 1;
		if( ((*(u32*)0x80000008)>>16) & 0xFF )
			bufferin[2] = 10;
		else 
			bufferin[2] = 0;
		}
	else
		{		
		bufferin[1] = 0;
		bufferin[2] = 0;
		}			
	DCFlushRange(bufferin, 0x20);
	
	int Ret = IOS_Ioctl(__dvd_fd, ioctl, bufferin, 0x20, bufferout, 0x20);
	
	IOS_Close(__dvd_fd);
	
	return ((Ret == 1) ? 0 : -Ret);
	}

static void SetGCVideoMode (void)
	{
	syssram *sram;
	sram = __SYS_LockSram();

	if(VIDEO_HaveComponentCable())
			sram->flags |= 0x80; //set progressive flag
	else
			sram->flags &= 0x7F; //clear progressive flag

	if (config.dmlvideomode == DMLVIDEOMODE_NTSC)
	{
			rmode = &TVNtsc480IntDf;
			sram->flags &= 0xFE; // Clear bit 0 to set the video mode to NTSC
			sram->ntd &= 0xBF; //clear pal60 flag
	}
	else
	{
			rmode = &TVPal528IntDf;
			sram->flags |= 0x01; // Set bit 0 to set the video mode to PAL
			sram->ntd |= 0x40; //set pal60 flag
	}

	__SYS_UnlockSram(1); // 1 -> write changes
	
	while(!__SYS_SyncSram());
	
	// TVPal528IntDf
	
	u32 *xfb;
	static GXRModeObj *rmode;
	
	//config.dmlvideomode = DMLVIDEOMODE_PAL;
	
	if (config.dmlvideomode == DMLVIDEOMODE_PAL)
		{
		rmode = &TVPal528IntDf;
		*(u32*)0x800000CC = VI_PAL;
		}
	else
		{
		rmode = &TVNtsc480IntDf;
		*(u32*)0x800000CC = VI_NTSC;
		}

	VIDEO_SetBlack(TRUE);
	VIDEO_Configure(rmode);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb);

	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	VIDEO_SetBlack(FALSE);
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	}

s32 StartMIOS (void)
	{
	s32 ret;
	
	//ret = setstreaming ();
	
	SetGCVideoMode ();
	
	tikview view ATTRIBUTE_ALIGN(32);
	
	ret = ES_GetTicketViews(BC, &view, 1);
	if (ret != 0) return -1;

	// Tell DML to boot the game from sd card
	*(u32 *)0x80001800 = 0xB002D105;
	DCFlushRange((void *)(0x80001800), 4);
	ICInvalidateRange((void *)(0x80001800), 4);			
	
	*(volatile unsigned int *)0xCC003024 |= 7;
	
	ret = ES_LaunchTitle(BC, &view);
	
	return -102;
	}
	
	
#define MAXGAMES 30
#define MAXROW 10

static bool GetName (char *path, char *id, char *name)
	{
	FILE *f;
	f = fopen(path, "rb");
	if (!f)	
		{
		*name = '\0';
		return false;
		}
	
	fread(id, 1, 8, f);
	//id[6] = 0;
	
	fseek( f, 0x20, SEEK_SET);
	fread(name, 1, 32, f);
	fclose(f);

	id[6]++;
	id[7]++;
	id[8] = 0;

	name[31] = 0;
	return true;
	}

int DMLRun (char *folder, char *id, u32 videomode)
	{
	char path[128];
	
	Debug ("DMLRun (%s, %s, %u)", folder, id, videomode);

	if (!devices_Get(DEV_SD)) return 0;
	
	if (videomode == PLGC_Auto)
		videomode = PLGC_Game;

	if (videomode == PLGC_PAL60)
		videomode = PLGC_PAL50;
	
	if (videomode == PLGC_Game) // GAME
		{
		if (id[3] == 'E' || id[3] == 'J' || id[3] == 'N')
			config.dmlvideomode = DMLVIDEOMODE_NTSC;
		else
			config.dmlvideomode = DMLVIDEOMODE_PAL;
		}
	if (videomode == PLGC_WII) // WII
		{
		if (CONF_GetRegion() == CONF_REGION_EU)
			config.dmlvideomode = DMLVIDEOMODE_PAL;
		else
			config.dmlvideomode = DMLVIDEOMODE_NTSC;
		}
	
	if (videomode == PLGC_NTSC)
		config.dmlvideomode = DMLVIDEOMODE_NTSC;

	if (videomode == PLGC_PAL50)
		config.dmlvideomode = DMLVIDEOMODE_PAL;

	sprintf (path, "%s://games/boot.bin", devices_Get(DEV_SD));
	
	FILE *f;
	f = fopen(path, "wb");
	if (!f)	return -1;
	fwrite(folder, 1, strlen(folder), f);
	fclose(f);
	
 	memcpy ((char *)0x80000000, id, 6);
	
	Shutdown (0);
	
	StartMIOS ();
	return 1;
	}

int DMLRunNew (char *folder, char *id, u8 videomode, u8 dmlNoDisc, u8 dmlPadHook)
	{
	DML_CFG cfg;
	
	Debug ("DMLRun (%s, %s, %u)", folder, id, videomode);

	if (!devices_Get(DEV_SD)) return 0;
	
	memset (&cfg, 0, sizeof (DML_CFG));
	
	cfg.Magicbytes = 0xD1050CF6;
	cfg.CfgVersion = 0x00000001;
		
	if (videomode == PLGC_Auto) // AUTO
		{
		cfg.VideoMode |= DML_VID_DML_AUTO;
		}
	if (videomode == PLGC_Game) // GAME
		{
		if (id[3] == 'E' || id[3] == 'J' || id[3] == 'N')
			cfg.VideoMode |= DML_VID_FORCE_NTSC;
		else
			cfg.VideoMode |= DML_VID_FORCE_PAL50;
		}
	if (videomode == PLGC_WII) // WII
		{
		if (CONF_GetRegion() == CONF_REGION_EU)
			cfg.VideoMode |= DML_VID_FORCE_PAL50;
		else
			cfg.VideoMode |= DML_VID_FORCE_NTSC;
		}
	
	if (videomode == PLGC_NTSC)
		cfg.VideoMode |= DML_VID_FORCE_NTSC;

	if (videomode == PLGC_PAL50)
		cfg.VideoMode |= DML_VID_FORCE_PAL50;

	if (videomode == PLGC_PAL60)
		cfg.VideoMode |= DML_VID_FORCE_PAL60;
		
	if (dmlNoDisc)
		cfg.Config |= DML_CFG_NODISC;

	if (dmlPadHook)
		cfg.Config |= DML_CFG_PADHOOK;

	strcpy (cfg.GamePath, folder);
 	memcpy ((char *)0x80000000, id, 6);
	
	Shutdown (0);

	/* Boot BC */
	WII_Initialize();
	return WII_LaunchTitle(0x100000100LL);
	}
	
///////////////////////////////////////////////////////////////////////////////////////////////////////	

void DMLResetCache (void)
	{
	char cachepath[128];
	sprintf (cachepath, "%s://ploader/dml.dat", vars.defMount);
	unlink (cachepath);
	}

#define BUFFSIZE (1024*64)

static void cb_DML (void)
	{
	Video_WaitPanel (TEX_HGL, "Please wait...|Searching gamecube games");
	}

char *DMLScanner  (bool reset)
	{
	static bool xcheck = true; // do that one time only
	DIR *pdir;
	struct dirent *pent;
	char cachepath[128];
	char path[128];
	char name[32];
	char src[32];
	char b[128];
	char id[10];
	FILE *f;
	char *buff = calloc (1, BUFFSIZE); // Yes, we are wasting space...
	
	sprintf (cachepath, "%s://ploader/dml.dat", vars.defMount);

	//reset = 1;

	if (reset == 0)
		{
		f = fopen (cachepath, "rb");
		if (!f) 
			{
			Debug ("DMLScanner: cache file '%s' not found", cachepath);
			reset = 1;
			}
		else
			{
			Debug ("DMLScanner: cache file '%s' found, checking version", cachepath);
			
			fread (b, 1, strlen(DMLVER), f);
			b[strlen(DMLVER)] = 0;
			if (strcmp (b, DMLVER) != 0)
				{
				Debug ("DMLScanner: version mismatch, forcing rebuild");
				reset = 1;
				}
			else
				fread (buff, 1, BUFFSIZE-1, f);
				
			fclose (f);
			
			buff[BUFFSIZE-1] = 0;
			}
		}
	
	if (reset == 1)
		{
		
		if (!devices_Get(DEV_SD)) return 0;
		
		sprintf (path, "%s://games", devices_Get(DEV_SD));
		
		fsop_GetFolderKb (path, 0);
		
		Debug ("DML: scanning %s", path);
		
		pdir=opendir(path);
		
		while ((pent=readdir(pdir)) != NULL) 
			{
			if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0) continue;
				
			sprintf (b, "%s/%s/game.iso", path, pent->d_name);
			
			if (!fsop_FileExist (b))
				sprintf (b, "%s/%s/sys/boot.bin", path, pent->d_name);
			
			Debug ("DML: checking %s", b);
			
			if (fsop_FileExist (b))
				{
				Video_WaitPanel (TEX_HGL, "Please wait...|Searching gamecube games");
				
				bool skip = false;
				
				if (xcheck && devices_Get(DEV_USB))
					{
					char sdp[256], usbp[256];
					
					sprintf (sdp, "%s://games/%s", devices_Get(DEV_SD), pent->d_name);
					
					int folder;
					for (folder = 0; folder < 2; folder++)
						{
						sprintf (usbp, "%s://%s/%s", devices_Get(DEV_USB), dmlFolders[folder], pent->d_name);
						
						if (fsop_DirExist (usbp))
							{
							int sdkb, usbkb;
							
							sdkb = fsop_GetFolderKb (sdp, cb_DML);
							usbkb = fsop_GetFolderKb (usbp, cb_DML);
							
							if (abs (sdkb - usbkb) > 5) // Let 5kb difference for codes
								{
								char mex[256];
								fsop_KillFolderTree (sdp, cb_DML);
								
								sprintf (mex, "Folder '%s' removed\n as it has the wrong size", sdp);
								grlib_menu (mex, "   OK   ");
								skip = true;
								}
							}
						}
					}
		
				if (!skip)
					{
					if (!GetName (b, id, name)) continue;
					
					//ms_strtoupper (pent->d_name);

					sprintf (b, "%s%c%s%c%d%c%s/%s%c", name, SEP, id, SEP, DEV_SD, SEP, path, pent->d_name, SEP);
					strcat (buff, b);
					}
				}
			}
			
		closedir(pdir);
		
		xcheck = false;

		int i;
		for (i = DEV_USB; i < DEV_MAX; i++)
			{
			if (devices_Get(i))
				{
				int folder;
				for (folder = 0; folder < 2; folder++)
					{
					sprintf (path, "%s://%s", devices_Get(i), dmlFolders[folder]);
					
					Debug ("DML: scanning %s", path);
					
					pdir=opendir(path);

					while ((pent=readdir(pdir)) != NULL) 
						{
						if (strcmp (pent->d_name, ".") && strcmp (pent->d_name, ".."))
							{
							ms_strtoupper (pent->d_name);

							Video_WaitPanel (TEX_HGL, "Please wait...|Searching gamecube games");
							
							sprintf (b, "%s/%s/game.iso", path, pent->d_name);

							if (!fsop_FileExist (b))
								sprintf (b, "%s/%s/sys/boot.bin", path, pent->d_name);

							Debug ("DML: checking %s", b);

							if (!GetName (b, id, name)) continue;
							
							sprintf (src, "%c%s%c", SEP, id, SEP); // make sure to find the exact name
							if (strstr (buff, src) == NULL)	// Make sure to not add the game twice
								{
								sprintf (b, "%s%c%s%c%d%c%s/%s%c", name, SEP, id, SEP, DEV_USB, SEP, path, pent->d_name, SEP);
								strcat (buff, b);
								}
							}
						}
						
					closedir(pdir);
					}
				}
			}

		Debug ("WBFSSCanner: writing cache file");
		f = fopen (cachepath, "wb");
		if (f) 
			{
			fwrite (DMLVER, 1, strlen(DMLVER), f);
			fwrite (buff, 1, strlen(buff)+1, f);
			fclose (f);
			}
		}

	int i, l;
	
	l = strlen (buff);
	for (i = 0; i < l; i++)
		if (buff[i] == SEP)
			buff[i] = 0;

	return buff;
	}
	
/*

DMLRemove will prompt to remove same games from sd to give space to new one

*/

int DMLInstall (char *gamename, size_t reqKb)
	{
	int ret;
	int i = 0, j;
	DIR *pdir;
	struct dirent *pent;
	char path[128];
	char menu[2048];
	char buff[64];
	char name[64];
	char title[256];
	
	char files[MAXGAMES][64];
	size_t sizes[MAXGAMES];

	size_t devKb = 0;
	
	Debug ("DMLInstall (%s): Started", gamename);

	if (!devices_Get(DEV_SD))
		{
		Debug ("DMLInstall (%s): ERR SD Device invalid", gamename);
		return 0;
		}
	
	sprintf (path, "%s://games", devices_Get(DEV_SD));

	devKb = fsop_GetFreeSpaceKb (path);
	
	Debug ("DMLInstall: devKb = %u, reqKb = %u", devKb, reqKb);
	
	if (devKb > reqKb) 
		{
		Debug ("DMLInstall (%s): OK there is enaught space", gamename);
		return 1; // We have the space
		}
	
	while (devKb < reqKb)
		{
		*menu = '\0';
		i = 0;
		j = 0;
		
		pdir=opendir(path);
		
		while ((pent=readdir(pdir)) != NULL) 
			{
			if (strlen (pent->d_name) ==  6)
				{
				strcpy (files[i], pent->d_name);
				
				GetName (DEV_SD, files[i], name);
				if (strlen (name) > 20)
					{
					name[12] = 0;
					strcat(name, "...");
					}
					
				sprintf (buff, "%s/%s", path, files[i]);
				sizes[i] = fsop_GetFolderKb (buff, NULL);
				grlib_menuAddItem (menu, i, "%s (%d Mb)", name, sizes[i] / 1000);
				
				i++;
				j++;
			
				if (j == MAXROW)
					{
					grlib_menuAddColumn (menu);
					j = 0;
					}
				
				if (i == MAXGAMES)
					{
					Debug ("DMLInstall (%s): Warning... to many games", gamename);

					break;
					}
				}
			}
		
		closedir(pdir);

		Debug ("DMLInstall (%s): found %d games on sd", gamename, i);
		
		sprintf (title, "You must free %u Mb to install %s\nClick on game to remove it from SD, game size is %u Mb), Press (B) to close", 
			(reqKb - devKb) / 1000, gamename, reqKb / 1000);

		ret = grlib_menu (title, menu);
		if (ret == -1)
			{
			Debug ("DMLInstall (%s): aborted by user", gamename);
			return 0;
			}
			
		if (ret >= 0)
			{
			char gamepath[128];
			sprintf (gamepath, "%s://games/%s", devices_Get(DEV_SD), files[ret]);
			
			Debug ("DMLInstall deleting '%s'", gamepath);
			fsop_KillFolderTree (gamepath, NULL);
			
			DMLResetCache (); // rebuild the cache next time
			}
			
		devKb = fsop_GetFreeSpaceKb (path);
		if (devKb > reqKb)
			{
			Debug ("DMLInstall (%s): OK there is enaught space", gamename);
			return 1; // We have the space
			}
		}
	
	Debug ("DMLInstall (%s): Something gone wrong", gamename);
	return 0;
	}
	
