#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/video_types.h>
#include <dirent.h>
#include <fcntl.h>
#include "globals.h"
#include "fsop/fsop.h"
#include "devices.h"
#include "mystring.h"
#include "stub.h"

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
	} 
DML_CFG;

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
	DML_CFG_NODISC          = (1<<9), //changed from NODISC to FORCE_WIDE in 2.1+, but will remain named NODISC until stfour drops support for old DM(L)
	DML_CFG_BOOT_DISC       = (1<<10),
    DML_CFG_BOOT_DISC2      = (1<<11), //changed from BOOT_DOL to BOOT_DISC2 in 2.1+
    DML_CFG_NODISC2         = (1<<12), //added in DM 2.2+
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
	
	u32 *sfb;
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
	sfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	VIDEO_ClearFrameBuffer(rmode, sfb, COLOR_BLACK);
	VIDEO_SetNextFramebuffer(sfb);

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
	
	Shutdown ();
	
	StartMIOS ();
	return 1;
	}

int DMLRunNew (char *folder, char *id, s_gameConfig *gameconf)
	{
	DML_CFG cfg;
	char path[256];
	
	memset (&cfg, 0, sizeof (DML_CFG));
	
	Debug ("DMLRunNew (%s, %s, %u, %u, %u, %u)", folder, id, gameconf->dmlVideoMode, gameconf->dmlNoDisc, gameconf->dmlPadHook, gameconf->dmlNMM);
	
	cfg.Config |= DML_CFG_GAME_PATH;

	if (config.dmlVersion == 2)
		sprintf (path, "usb:/%s/game.iso", folder);
	else
		sprintf (path, "sd:/%s/game.iso", folder);
		
	if (fsop_FileExist (path))
		{
		sprintf (path, "%s/game.iso", folder);
		}
	else
		{
		sprintf (path, "%s/", folder);
		}

	Debug ("DMLRunNew -> using %s", path);

	//if (!devices_Get(DEV_SD)) return 0;
	
	Shutdown ();

	cfg.Magicbytes = 0xD1050CF6;
	if (config.dmlVersion == GCMODE_DM22)
		cfg.CfgVersion = 0x00000002;
	else
		cfg.CfgVersion = 0x00000001;
		
	if (gameconf->dmlVideoMode == PLGC_Auto) // AUTO
		{
		cfg.VideoMode |= DML_VID_DML_AUTO;
		}
	if (gameconf->dmlVideoMode == PLGC_Game) // GAME
		{
		if (id[3] == 'E' || id[3] == 'J' || id[3] == 'N')
			cfg.VideoMode |= DML_VID_FORCE_NTSC;
		else
			cfg.VideoMode |= DML_VID_FORCE_PAL50;
		}
	if (gameconf->dmlVideoMode == PLGC_WII) // WII
		{
		if (CONF_GetRegion() == CONF_REGION_EU)
			cfg.VideoMode |= DML_VID_FORCE_PAL50;
		else
			cfg.VideoMode |= DML_VID_FORCE_NTSC;
		}
	
	if (gameconf->dmlVideoMode == PLGC_NTSC)
		cfg.VideoMode |= DML_VID_FORCE_NTSC;

	if (gameconf->dmlVideoMode == PLGC_PAL50)
		cfg.VideoMode |= DML_VID_FORCE_PAL50;

	if (gameconf->dmlVideoMode == PLGC_PAL60)
		cfg.VideoMode |= DML_VID_FORCE_PAL60;
		
	//kept as nodisc for legacy purposes, but it also controls
	//widescreen force 16:9 in 2.1+
	if (gameconf->dmlNoDisc)
		cfg.Config |= DML_CFG_NODISC;

	if (gameconf->dmlFullNoDisc)
		cfg.Config |= DML_CFG_NODISC2;

	if (gameconf->dmlPadHook)
		cfg.Config |= DML_CFG_PADHOOK;

	if (gameconf->dmlNMM)
		cfg.Config |= DML_CFG_NMM;
		
    if(gameconf->ocarina)
        cfg.Config |= DML_CFG_CHEATS;

	strcpy (cfg.GamePath, path);
 	memcpy ((char *)0x80000000, id, 6);
	
	//Write options into memory
	memcpy((void *)0x80001700, &cfg, sizeof(DML_CFG));
	DCFlushRange((void *)(0x80001700), sizeof(DML_CFG));

	//DML v1.2+
	memcpy((void *)0x81200000, &cfg, sizeof(DML_CFG));
	DCFlushRange((void *)(0x81200000), sizeof(DML_CFG));

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

/*
static void cb_DML (void)
	{
	Video_WaitPanel (TEX_HGL, "Please wait...|Searching gamecube games");
	}
*/

char *DMLScanner  (bool reset)
	{
	//static bool xcheck = true; // do that one time only
	DIR *pdir;
	struct dirent *pent;
	char cachepath[128];
	char path[128];
	char fullpath[128];
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
		// Allow usb only for DM
		if (config.dmlVersion < GCMODE_DM22 && !devices_Get(DEV_SD)) return 0;
		
		if (config.dmlVersion != GCMODE_DM22 && devices_Get(DEV_SD))
			{
			sprintf (path, "%s://games", devices_Get(DEV_SD));
			
			Debug ("DML: scanning %s", path);
			
			pdir=opendir(path);
			
			while ((pent=readdir(pdir)) != NULL) 
				{
				if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0) continue;
				
				int found = 0;
				if (config.dmlVersion == GCMODE_DEVO)
					{
					sprintf (fullpath, "%s/%s", path, pent->d_name);
					if (ms_strstr (fullpath, ".iso") && fsop_FileExist (fullpath))
						found = 1;
					}
				
				if (!found)
					{
					sprintf (fullpath, "%s/%s/game.iso", path, pent->d_name);
					found = fsop_FileExist (fullpath);
					}
				
				if (!found && config.dmlVersion != GCMODE_DEVO)
					{
					sprintf (fullpath, "%s/%s/sys/boot.bin", path, pent->d_name);
					found = fsop_FileExist (fullpath);
					}
					
				Debug ("DML: checking %s", fullpath);
				
				if (found)
					{
					Video_WaitPanel (TEX_HGL, "Please wait...|Searching gamecube games");
					
					bool skip = false;
					
					/*
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
					*/
					
					if (!skip)
						{
						if (!GetName (fullpath, id, name)) continue;
						
						//ms_strtoupper (pent->d_name);
						if (config.dmlVersion != GCMODE_DEVO)
							sprintf (b, "%s%c%s%c%d%c%s/%s%c", name, SEP, id, SEP, DEV_SD, SEP, path, pent->d_name, SEP);
						else
							sprintf (b, "%s%c%s%c%d%c%s%c", name, SEP, id, SEP, DEV_SD, SEP, fullpath, SEP);
						
						strcat (buff, b);
						}
					}
				}
				
			closedir(pdir);
			}
		
		//xcheck = false;

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
						if (strcmp (pent->d_name, ".") == 0 || strcmp (pent->d_name, "..") == 0) continue;
						
						Video_WaitPanel (TEX_HGL, "Please wait...|Searching gamecube games");
						
						ms_strtoupper (pent->d_name);
						
						int found = 0;
						if (config.dmlVersion == GCMODE_DEVO)
							{
							sprintf (fullpath, "%s/%s", path, pent->d_name);
							if (ms_strstr (fullpath, ".iso") && fsop_FileExist (fullpath))
								found = 1;
							}
						
						if (!found)
							{
							sprintf (fullpath, "%s/%s/game.iso", path, pent->d_name);
							found = fsop_FileExist (fullpath);
							}
						
						if (!found && config.dmlVersion != GCMODE_DEVO)
							{
							sprintf (fullpath, "%s/%s/sys/boot.bin", path, pent->d_name);
							found = fsop_FileExist (fullpath);
							}
							
						Debug ("DML: checking %s", fullpath);
						
						if (!found || !GetName (fullpath, id, name)) continue;
						
						Debug ("DML: valid image!");
						
						sprintf (src, "%c%s%c", SEP, id, SEP); // make sure to find the exact name
						if (strstr (buff, src) == NULL)	// Make sure to not add the game twice
							{
							if (config.dmlVersion != GCMODE_DEVO)
								sprintf (b, "%s%c%s%c%d%c%s/%s%c", name, SEP, id, SEP, DEV_USB, SEP, path, pent->d_name, SEP);
							else
								sprintf (b, "%s%c%s%c%d%c%s%c", name, SEP, id, SEP, DEV_USB, SEP, fullpath, SEP);
							
							strcat (buff, b);
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
	

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Devolution: based on FIX94 implementation in WiiFlow
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// constant value for identification purposes
#define CONFIG_SIG        0x3EF9DB23

// version may change when future options are added
#define CONFIG_VERSION    0x0100

typedef struct global_config
{
	uint32_t signature;
	uint16_t version;
	uint16_t device_signature;
	uint32_t memcard_cluster;
	uint32_t disc1_cluster;
	uint32_t disc2_cluster;
} gconfig; 

u8 *loader_bin = NULL;
static gconfig *DEVO_CONFIG = (gconfig*)0x80000020;

static bool IsGCCardAvailable (void)
	{
	CARD_Init (NULL, NULL);
    return (CARD_Probe (CARD_SLOTA) <= 0) ? false : true;
	}

// path is the full path to iso image
bool DEVO_Boot (char *path)
	{       
	//Read in loader.bin
	char loader_path[256];
	
	Debug ("DEVO_Boot: %s", path);
		
	snprintf(loader_path, sizeof (loader_path), "%s://ploader/plugins/loader.bin", vars.defMount);
	
	loader_bin = fsop_ReadFile (loader_path, 0, NULL);
	if (!loader_bin) return false;
	
	Debug ("DEVO_Boot: loader in memory");
	
	//start writing cfg to mem
	struct stat st;
	char full_path[256];
	int data_fd;
	char gameID[7];

	FILE *f = fopen(path, "rb");
	if (!f)
		{
		free (loader_bin);
		return false;
		}
		
	fread ((u8*)0x80000000, 1, 32, f);
	fclose (f);

	memcpy (&gameID, (u8*)0x80000000, 6);

	stat (path, &st);

	// fill out the Devolution config struct
	memset(DEVO_CONFIG, 0, sizeof(*DEVO_CONFIG));
	DEVO_CONFIG->signature = 0x3EF9DB23;
	DEVO_CONFIG->version = 0x00000100;
	DEVO_CONFIG->device_signature = st.st_dev;
	DEVO_CONFIG->disc1_cluster = st.st_ino;

	// make sure these directories exist, they are required for Devolution to function correctly
	snprintf(full_path, sizeof(full_path), "%s:/apps", fsop_GetDev (path));
	fsop_MakeFolder(full_path);
	
	snprintf(full_path, sizeof(full_path), "%s:/apps/gc_devo", fsop_GetDev (path));
	fsop_MakeFolder(full_path);

	if (!IsGCCardAvailable ())
		{
		Debug ("DEVO_Boot: using emulated card");
		
		// find or create a 16MB memcard file for emulation
		// this file can be located anywhere since it's passed by cluster, not name
		// it must be at least 16MB though
		if(gameID[3] == 'J') //Japanese Memory Card
			snprintf(full_path, sizeof(full_path), "%s:/apps/gc_devo/memcard_jap.bin", fsop_GetDev (path));
		else
			snprintf(full_path, sizeof(full_path), "%s:/apps/gc_devo/memcard.bin", fsop_GetDev (path));

		// check if file doesn't exist or is less than 16MB
		if (stat(full_path, &st) == -1 || st.st_size < 16<<20)
			{
			// need to enlarge or create it
			data_fd = open(full_path, O_WRONLY|O_CREAT);
			if (data_fd >= 0)
				{
				// make it 16MB
				gprintf("Resizing memcard file...\n");
				ftruncate(data_fd, 16<<20);
				if (fstat(data_fd, &st) == -1 || st.st_size < 16<<20)
					{
					// it still isn't big enough. Give up.
					st.st_ino = 0;
					}
				close(data_fd);
				}
			else
				{
				// couldn't open or create the memory card file
				st.st_ino = 0;
				}
			}
		}
	else
		{
		Debug ("DEVO_Boot: using real card");
		st.st_ino = 0;
		}

	// set FAT cluster for start of memory card file
	// if this is zero memory card emulation will not be used
	DEVO_CONFIG->memcard_cluster = st.st_ino;

	// flush disc ID and Devolution config out to memory
	DCFlushRange((void*)0x80000000, 64);
	
	Shutdown ();

	// Configure video mode as "suggested" to devolution
	GXRModeObj *vidmode;

	if (gameID[3] == 'E' || gameID[3] == 'J')
		vidmode = &TVNtsc480IntDf;
	else
		vidmode = &TVPal528IntDf;
		
	static u8 *sfb = NULL;
	sfb = SYS_AllocateFramebuffer(vidmode);
	VIDEO_ClearFrameBuffer(vidmode, sfb, COLOR_BLACK);
	sfb = MEM_K0_TO_K1(sfb);
	VIDEO_Configure(vidmode);
	VIDEO_SetNextFramebuffer(sfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();

	// the Devolution blob has an ID string at offset 4
	gprintf((const char*)loader_bin + 4);

	// devolution seems to like hbc stub. So we can force it.
	((void(*)(void))loader_bin)();

	return true;
	}


