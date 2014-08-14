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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Devolution: based on FIX94 implementation in WiiFlow
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// constant value for identification purposes
#define CONFIG_SIG        0x3EF9DB23

// version may change when future options are added
#define CONFIG_VERSION    0x0110

typedef struct global_config
{
	uint32_t signature;
	uint16_t version;
	uint16_t device_signature;
	uint32_t memcard_cluster;
	uint32_t disc1_cluster;
	uint32_t disc2_cluster;
	u32 options;
} gconfig; 

// constant value for identification purposes
#define DEVO_CONFIG_SIG                 0x3EF9DB23
// version may change when future options are added
#define DEVO_CONFIG_VERSION             0x0110
// option flags
#define DEVO_CONFIG_WIFILOG             (1<<0)
#define DEVO_CONFIG_WIDE                (1<<1)
#define DEVO_CONFIG_NOLED               (1<<2)

u8 *loader_bin = NULL;
static gconfig *DEVO_CONFIG = (gconfig*)0x80000020;

static bool IsGCCardAvailable (void)
	{
	CARD_Init (NULL, NULL);
    return (CARD_Probe (CARD_SLOTA) <= 0) ? false : true;
	}

// path is the full path to iso image
bool DEVO_Boot (char *path, u8 memcardId, bool widescreen, bool activity_led, bool wifi)
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
	DEVO_CONFIG->signature = DEVO_CONFIG_SIG;
	DEVO_CONFIG->version = DEVO_CONFIG_VERSION;
	DEVO_CONFIG->device_signature = st.st_dev;
	DEVO_CONFIG->disc1_cluster = st.st_ino;

      // Pergame options
	if(wifi)
			DEVO_CONFIG->options |= DEVO_CONFIG_WIFILOG;
	if(widescreen)
			DEVO_CONFIG->options |= DEVO_CONFIG_WIDE;
	if(!activity_led)
			DEVO_CONFIG->options |= DEVO_CONFIG_NOLED;

	// make sure these directories exist, they are required for Devolution to function correctly
	snprintf(full_path, sizeof(full_path), "%s:/apps", fsop_GetDev (path));
	fsop_MakeFolder(full_path);
	
	snprintf(full_path, sizeof(full_path), "%s:/apps/gc_devo", fsop_GetDev (path));
	fsop_MakeFolder(full_path);

	if (!IsGCCardAvailable ())
		{
		char cardname[64];
		
		if (memcardId == 0)
			{
			if(gameID[3] == 'J') //Japanese Memory Card
				sprintf (cardname, "memcard_jap.bin");
			else
				sprintf (cardname, "memcard.bin");
			}
		else
			{
			if(gameID[3] == 'J') //Japanese Memory Card
				sprintf (cardname, "memcard%u_jap.bin", memcardId);
			else
				sprintf (cardname, "memcard%u.bin", memcardId);
			}
		
		Debug ("DEVO_Boot: using emulated card");
		
		// find or create a memcard file for emulation(as of devolution r115 it doesn't need to be 16MB)
		// this file can be located anywhere since it's passed by cluster, not name
		if(gameID[3] == 'J') //Japanese Memory Card
			snprintf(full_path, sizeof(full_path), "%s:/apps/gc_devo/%s", fsop_GetDev (path), cardname);
		else
			snprintf(full_path, sizeof(full_path), "%s:/apps/gc_devo/%s", fsop_GetDev (path), cardname);

		// check if file doesn't exist
		if (stat(full_path, &st) == -1)
			{
			// need to create it
			data_fd = open(full_path, O_WRONLY|O_CREAT);
			if (data_fd >= 0)
				{
				// make it 16MB, if we're creating a new memory card image
				//gprintf("Resizing memcard file...\n");
				ftruncate(data_fd, 16<<20);
				if (fstat(data_fd, &st) == -1)
						{
						// it still isn't created. Give up.
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


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Nintendont support: The structs and enums are borrowed from Nintendont (as is IsWiiU).
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NIN_CFG_VERSION 0x00000002
#define NIN_MAGIC 0x01070CF6
#define NIN_CFG_MAXPADS 4

	typedef struct NIN_CFG
	{
		unsigned int	Magicbytes;
		unsigned int	Version;
		unsigned int	Config;
		unsigned int	VideoMode;
		unsigned int	Language;
		char	GamePath[255];
		char	CheatPath[255];
		unsigned int	MaxPads;
		unsigned int	GameID;
	} NIN_CFG;

	enum ninconfig
	{
		NIN_CFG_CHEATS = (1 << 0),
		NIN_CFG_DEBUGGER = (1 << 1),	// Only for Wii Version
		NIN_CFG_DEBUGWAIT = (1 << 2),	// Only for Wii Version
		NIN_CFG_MEMCARDEMU = (1 << 3),
		NIN_CFG_CHEAT_PATH = (1 << 4),
		NIN_CFG_FORCE_WIDE = (1 << 5),
		NIN_CFG_FORCE_PROG = (1 << 6),
		NIN_CFG_AUTO_BOOT = (1 << 7),
		NIN_CFG_HID = (1 << 8),
		NIN_CFG_OSREPORT = (1 << 9),
		NIN_CFG_USB = (1 << 10),
		NIN_CFG_LED = (1 << 11),
	};

	enum ninvideomode
	{
		NIN_VID_AUTO = (0 << 16),
		NIN_VID_FORCE = (1 << 16),

		NIN_VID_FORCE_PAL50 = (1 << 0),
		NIN_VID_FORCE_PAL60 = (1 << 1),
		NIN_VID_FORCE_NTSC = (1 << 2),
		NIN_VID_FORCE_MPAL = (1 << 3),

		NIN_VID_PROG = (1 << 4),	//important to prevent blackscreens
	};

	enum
	{
		VID_AUTO = 0,
		VID_NTSC = 1,
		VID_MPAL = 2,
		VID_PAL50 = 3,
		VID_PAL60 = 4,
	};

	// Borrowed from Nintendont, renamed from IsWiiU
	bool RunningOnWiiU(void)
	{
		return ((*(vu32*)(0xCd8005A0) >> 16) == 0xCAFE);
	}

	char *NIN_GetLanguage(int language)
	{
		static char *languageoptions[6] = { "English", "German", "French", "Spanish", "Italian", "Dutch" };

		return language < 0 || language >= ARRAY_LENGTH(languageoptions) ? "Auto" : languageoptions[language];
	}

	bool NIN_Boot(s_game *game, s_gameConfig *gameConf, char *error_string, int error_strlen)
	{
		if (game == NULL)
			{
			Debug ("NIN_Boot: game is null!");
			const char * const error = "An internal error occurred. Game info is null.";
			strncpy (error_string, error, strlen(error));
			return false;
			}

		if (gameConf == NULL)
			{
			Debug ("NIN_Boot: gameConf is null!");
			const char * const error = "An internal error occurred. Game config is null.";
			strncpy (error_string, error, strlen(error));
			return false;
			}

		Debug ("NIN_Boot: preparing to launch %s", game->name);

		NIN_CFG nin_config = { 0 };
		nin_config.Magicbytes = NIN_MAGIC;
		nin_config.Version = NIN_CFG_VERSION;

		nin_config.Config = NIN_CFG_AUTO_BOOT;

		/*
		   OSReport is only useful for USB Gecko users.
		   In the future, if Nintendont gains wifi logging, and the user enables it, we'll toggle it on as well as well.
		   For now, however, wifi is commented out. 
		*/
		if (!RunningOnWiiU () && (usb_isgeckoalive (EXI_CHANNEL_1) /* || gameConf->wifi*/ ))
			nin_config.Config |= NIN_CFG_OSREPORT;

		const bool gameIsOnUSB = strstr (game->source, "usb:/") != NULL;

		if (gameIsOnUSB)
			{
			Debug ("NIN_Boot: game is on USB.");
			nin_config.Config |= NIN_CFG_USB;
			}
		else
			Debug ("NIN_Boot: game is on SD.");

		if (gameConf->dmlNMM)
			{
			Debug ("NIN_Boot: enabling MC emulation.");
			nin_config.Config |= NIN_CFG_MEMCARDEMU;
			}

		if (gameConf->widescreen)
			{
			Debug ("NIN_Boot: forcing widescreen.");
			nin_config.Config |= NIN_CFG_FORCE_WIDE;
			}

		if (gameConf->activity_led)
			{
			Debug ("NIN_Boot: activity LED will be active.");
			nin_config.Config |= NIN_CFG_LED;
			}

		// On Wii U, we have to force this, because there are no GC controllers.
		if (RunningOnWiiU() || gameConf->hidController)
			{
			Debug ("NIN_Boot: HID controllers will be used.");
			nin_config.Config |= NIN_CFG_HID;
			}

		const bool progressive = (CONF_GetProgressiveScan () > 0) && VIDEO_HaveComponentCable ();
		if (progressive) //important to prevent blackscreens
			{
			Debug ("NIN_Boot: this %s seems to be able to use progressive mode.", RunningOnWiiU() ? "Wii U" : "Wii");
			nin_config.VideoMode |= NIN_VID_PROG;
			}
		else
			nin_config.VideoMode &= ~NIN_VID_PROG;

		int force = 0;

		switch (gameConf->dmlVideoMode)
		{
		case VID_NTSC:
			force = NIN_VID_FORCE | NIN_VID_FORCE_NTSC;
			Debug ("NIN_Boot: forcing video mode to NTSC.");
			break;
		case VID_MPAL:
			force = NIN_VID_FORCE | NIN_VID_FORCE_MPAL;
			Debug ("NIN_Boot: forcing video mode to MPAL.");
			break;
		case VID_PAL50:
			force = NIN_VID_FORCE | NIN_VID_FORCE_PAL50;
			Debug ("NIN_Boot: forcing video mode to PAL50.");
			break;
		case VID_PAL60:
			force = NIN_VID_FORCE | NIN_VID_FORCE_PAL60;
			Debug ("NIN_Boot: forcing video mode to PAL60.");
			break;
		default:
			force = NIN_VID_AUTO;
			Debug ("NIN_Boot: letting Nintendont decide on the video mode.");
			break;
		}

		nin_config.VideoMode |= force;
		
		nin_config.Language = gameConf->ninLanguage;
		Debug ("NIN_Boot: game language set to %s.", NIN_GetLanguage(nin_config.Language));

		/*
			Nintendont expects the path to look something like this (we use /games since most other loaders do, too):
			"/games/<game id or folder name>/game.iso", without the boot device (e.g. "usb:/", "sd:/"),
			so we skip it as we build the path.
		*/

		// It should be safe to assume that strstr isn't null here.
		const char * const gamesPath = strstr (game->source, "/games/");
		sprintf (nin_config.GamePath, "%s/game.iso", gamesPath);
		
		Debug ("NIN_Boot: using %s as game ISO path.", nin_config.GamePath);

		// Just let Nintendont handle the cheat path.
		if (gameConf->ocarina)
			{
			nin_config.Config |= NIN_CFG_CHEATS;
			Debug ("NIN_Boot: cheats are enabled. Nintendont will decide on the cheat path.");
			}

		nin_config.MaxPads = NIN_CFG_MAXPADS;

		memcpy (&nin_config.GameID, game->asciiId, sizeof(int));
		Debug ("NIN_Boot: game ID is %s.", game->asciiId);

		/*
			Write Nintendont's config to storage.
			This basically automatically clears it out on every boot. 
		*/
		char cfgPath[256] = { 0 };
		sprintf(cfgPath, "%s://%s", gameIsOnUSB ? "usb" : "sd", "nincfg.bin");
		Debug ("NIN_Boot: using %s as config path.", cfgPath);

		if (!fsop_WriteFile (cfgPath, (u8 *)&nin_config, sizeof(NIN_CFG)))
			{
			Debug ("NIN_Boot: unable to write config file %s.", cfgPath);
			const char * const error = "postLoader was unable to write the config file nincfg.bin.";
			strncpy (error_string, error, strlen(error));
			return false;
			}

		// Clear out Nintendont's log file just in case.
		char logPath[256] = { 0 };
		sprintf (logPath, "%s://%s", gameIsOnUSB ? "usb" : "sd", "ndebug.log");
		Debug ("NIN_Boot: using %s as log path.", logPath);

		if (!fsop_WriteFile (logPath, NULL, 0))
			{
			Debug ("NIN_Boot: Unable to write blank log file %s.", logPath);
			const char * const error = "postLoader was unable to clear out ndebug.log.";
			strncpy (error_string, error, strlen(error));
			return false;
			}

		// Prepare to boot Nintendont!
		char ninPath[256] = { 0 };
		sprintf (ninPath, "%s://apps/nintendont/boot.dol", gameIsOnUSB ? "usb" : "sd");
		Debug ("NIN_Boot: looking for Nintendont at %s.", ninPath);

		if (!fsop_FileExist (ninPath))
			{
			Debug ("NIN_Boot: unable to find Nintendont at %s.", ninPath);
			const char * const error = "Nintendont doesn't seem to be installed in /apps/nintendont.";
			strncpy (error_string, error, strlen(error));
			return false;
			}

		return DirectDolBoot (ninPath, NULL, 0);
	}
