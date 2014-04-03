#ifndef __GLOBALS__
#define __GLOBALS__

#include <unistd.h>
#include <wiiuse/wpad.h>
#include "fsop/fsop.h"

#include "grlib/grlib.h"
#include "plneek.h"
#include "debug.h"
#include "cfg.h"

//#define DOLPHINE

#define VER "4.7.4"
#define CFGVER "PLCFGV0017" //PLCFGV0016 4.2.0 
#define HBCFGVER 1
#define IOS_CIOS 249
#define IOS_PREFERRED 58
#define IOS_SNEEK 56

#define EXTCONFIG "$EXTCONF$>0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF<$EXTCONF$"
#define EXTCONFIGOFFS 10
#define EXTCONFIGSIZE 256

#define HBAPPFOLDER "/apps/postloader"

#define TTFVERYSMALL 17
#define TTFVERYSMALL_fontOffsetY -4
#define TTFVERYSMALL_fontSizeOffsetY -3

#define TTFSMALL 18
#define TTFSMALL_fontOffsetY -7
#define TTFSMALL_fontSizeOffsetY -6

#define TTFNORM 22
#define TTFNORM_fontOffsetY -6
#define TTFNORM_fontSizeOffsetY -5

#define CFG_HOME_PRIILOAER 0x4461636F
#define PRIILOADER_TOMENU  0x50756E65

#define DMLVIDEOMODE_NTSC 0
#define DMLVIDEOMODE_PAL 1

#define GM_WII 0
#define GM_DML 1

#define APPCATS_MAX 8

#define TITLE_UPPER(x)		((u32)((x) >> 32))
#define TITLE_LOWER(x)		((u32)(x))
#define TITLE_ID(x,y)		(((u64)(x) << 32) | (y))

enum {
	DMLWAD_UNKNOWN = 0,
	DMLWAD_DM,
	DMLWAD_DML,
	DMLWAD_QFSD,
	DMLWAD_QFUSB,
	DMLWAD_MIOS,
	DMLWAD_CMIOS,
	DMLWAD_MAX,
	};

enum {
	INTERACTIVE_RET_NONE=0,

	INTERACTIVE_RET_TOCHANNELS,
	INTERACTIVE_RET_TOHOMEBREW,
	INTERACTIVE_RET_TOGAMES,
	INTERACTIVE_RET_TOEMU,

	INTERACTIVE_RET_HBSEL,
	INTERACTIVE_RET_CHANSEL,
	INTERACTIVE_RET_GAMESEL,
	INTERACTIVE_RET_SHUTDOWN,
	INTERACTIVE_RET_WIIMENU,
	INTERACTIVE_RET_BOOTMII,
	INTERACTIVE_RET_NEEK2O,
	INTERACTIVE_RET_WIILOAD,
	INTERACTIVE_RET_DISC,
	INTERACTIVE_RET_SE,
	INTERACTIVE_RET_WM,
	INTERACTIVE_RET_HBC,
	INTERACTIVE_RET_REBOOT
	};

enum {
	GCMODE_DML0x = 0,
	GCMODE_DML1x,
	GCMODE_DM22,
	GCMODE_DEVO,
	GCMODE_DMAUTO,
	GCMODE_MAX
	};

#define WAITPANWIDTH 300

#define PATHMAX 300
#define ARGSMAX 300

typedef void (*entrypoint) (void); 

#define CATAPPMAX 8			// number of category for applications
#define EMUPINMAX 256		// number of loadable plugins

#define APPMODE_NONE 0  	// Homebrew application
#define APPMODE_HBA 1  	 	// Homebrew application
#define APPMODE_CHAN 2  	// Channel on real nand

#define RETURNTO_AUTOBOOT 0
#define RETURNTO_BROWSER 1

#define PWDMAXLEN 8

#define BROWSE_HB 0
#define BROWSE_CH 1
#define BROWSE_GM 2
#define BROWSE_EM 3

// Global menuitem
#define MENU_SHAREDITEMS 9999
#define MENU_CHANGENEEK2o MENU_SHAREDITEMS + 1

// These show reflect what is declared in triiforce
#define NAND_REAL 0
#define NAND_EMUSD 1
#define NAND_EMUUSB 2

#define CHANNELS_MAXFILTERS 12

#define GAMEFLT_PLATFORM 		(1 <<  0)
#define GAMEFLT_ACTION			(1 <<  1)
#define GAMEFLT_SPORT			(1 <<  2)
#define GAMEFLT_TRAINING		(1 <<  3)
#define GAMEFLT_MUSIC			(1 <<  4)
#define GAMEFLT_SURVIVAL		(1 <<  5)
#define GAMEFLT_RACE			(1 <<  6)
#define GAMEFLT_PARTY			(1 <<  7)
#define GAMEFLT_SHOOTING		(1 <<  8)
#define GAMEFLT_UDRAW			(1 <<  9)
#define GAMEFLT_INSTRUMENT		(1 << 10)
#define GAMEFLT_BALANCE			(1 << 11)

enum {
	TEX_CUR=0,
	TEX_HDD,
	TEX_HGL,
	TEX_RUN,
	TEX_CHIP,
	TEX_WIFI,
	TEX_WIFIGK,
	TEX_GK,
	TEX_DVD,

	// for browser
	TEX_STAR,
	TEX_NONE,
	TEX_CHECKED,
	TEX_FOLDER,
	TEX_FOLDERUP, 
	TEX_GHOST,
	TEX_EXCL,
	TEX_USB,
	TEX_SD,
	
	TEX_CAT_HB,
	TEX_CAT_WIIWARE,
	TEX_CAT_WII,
	TEX_CAT_GAMECUBE,
	TEX_CAT_EMUL,

	TEX_ICO_ABOUT,
	TEX_ICO_CONFIG,
	TEX_ICO_DVD,
	TEX_ICO_EXIT,
	TEX_ICO_NEEK,
	TEX_ICO_SE,
	TEX_ICO_WM,

	MAXTEX
	};

// Version of neek (none, sneek, uneek)
#define NEEK_NONE 0
#define NEEK_SD 1
#define NEEK_USB 2
#define NEEK_2o 3

typedef void (*voidCallback)(void); 

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct
	{
	char asciiId[6];// id in ascii format
	u64 titleId; 	// title id
	u32 priority;	// Vote !
	bool hidden;	// if 1, this app will be not listed
	
	u8 ios;		 	// ios to reload
	u8 vmode;	 	// 0 Default Video Mode	1 NTSC480i	2 NTSC480p	3 PAL480i	4 PAL480p	5 PAL576i	6 MPAL480i	7 MPAL480p
	s8 language; 	//	-1 Default Language	0 Japanese	1 English	2 German	3 French	4 Spanish	5 Italian	6 Dutch	7 S. Chinese	8 T. Chinese	9 Korean
	u8 vpatch;	 	// 0 No Video patches	1 Smart Video patching	2 More Video patching	3 Full Video patching
	u8 hook;	 	// 0 No Ocarina&debugger	1 Hooktype: VBI	2 Hooktype: KPAD	3 Hooktype: Joypad	4 Hooktype: GXDraw	5 Hooktype: GXFlush	6 Hooktype: OSSleepThread	7 Hooktype: AXNextFrame
	u8 ocarina; 	// 0 No Ocarina	1 Ocarina from NAND 	2 Ocarina from SD	3 Ocarina from USB"
	u8 bootMode;	// 0 Normal boot method	1 Load apploader
	u16 playcount;	// how many time this title has bin executed
	}
s_channelConfig;

typedef struct
	{
	u64 titleId; 			// title id
	char asciiId[6];		// id in ascii format
	char *name;				// name
	u8 *png;				// Address of png in cache area
	size_t pngSize;

	bool filtered;			// if 1, this app match the filter
	bool needUpdate;
	bool checked;
	u8 hasCover;			// if != 0 this rom has it own cover

	// These are updated from s_channelConfig when browsing
	int priority;	// Vote !
	bool hidden;	// if 1, this app will be not listed
	}
s_channel;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct
	{
	int priority;	// Vote !
	u8 hidden;	// if 1, this app will be not listed
	
	char asciiId[8];// id in ascii format (6 needed)
	char name[128];// id in ascii format (6 needed)
	u8 ios;		 	// ios to reload
	u8 vmode;	 	// 0 Default Video Mode	1 NTSC480i	2 NTSC480p	3 PAL480i	4 PAL480p	5 PAL576i	6 MPAL480i	7 MPAL480p
	s8 language; 	//	-1 Default Language	0 Japanese	1 English	2 German	3 French	4 Spanish	5 Italian	6 Dutch	7 S. Chinese	8 T. Chinese	9 Korean
	u8 vpatch;	 	// 0 No Video patches	1 Smart Video patching	2 More Video patching	3 Full Video patching
	u8 hook;	 	// 0 No Ocarina&debugger	1 Hooktype: VBI	2 Hooktype: KPAD	3 Hooktype: Joypad	4 Hooktype: GXDraw	5 Hooktype: GXFlush	6 Hooktype: OSSleepThread	7 Hooktype: AXNextFrame
	u8 ocarina; 	// 0 No Ocarina	1 Ocarina from NAND 	2 Ocarina from SD	3 Ocarina from USB"
	
	u8 nand;		// neek nand index  0:"Default", "USA" , "EURO", "JAP", "Korean"
	u8 loader;		// 0 cfg, 1 gx, 2 wiiflow
	u16 playcount;	// how many time this title has bin executed
	u32 category;	// bitmask category
	u8 minPlayerAge;
	
	u8 dmlVideoMode;	// Current video mode for dml
	u8 dmlNoDisc;		// nodisc patch
	u8 dmlFullNoDisc;	// new nodisc patches added in dml 2.1+
	u8 dmlPadHook;		// padhook patch
	u8 dmlNMM;			// nmm patch

	u8 memcardId;		// this is for devolution, and maybe dm(l) in future... postloader support creation/managment of up 8 card images, selecatable x game
	u8 widescreen;		// this should be for devolution, will force 16/9 mode
	u8 wifi;			// this should be for devolution, will force 16/9 mode
	u8 activity_led;
	}
s_gameConfig;

typedef struct
	{
	u32 slot;				// under neeek, this is the slot, in real this is the partition, in dml if 1 it is on usb device
	char asciiId[8];		// id in ascii format (6 needed)
	char *name;				// name
	u8 *png;				// Address of png in cache area
	size_t pngSize;

	bool filtered;			// if 1, this app match the filter
	bool needUpdate;
	bool checked;
	u8 hasCover;			// if != 0 this rom has it own cover

	// These are updated from s_channelConfig when browsing
	int priority;			// Vote !
	bool hidden;			// if 1, this app will be not listed
	u32 category;
	u16 playcount;			// how many time this title has bin executed
	
	bool dml;				// if true we are executing a dml game
	char source[96];		// source folder
	u8 disc;
	}
s_game;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct
	{
	char name[128];
	u8 priority;	// Vote !
	u8 hidden;		// if 1, this app will be not listed
	u16 playcount;	// how many time this title has bin executed
	}
s_emuConfig;

typedef struct
	{
	u8 type;				// plugin type...
	char *name;				// name
	char *cover;			// covername, if NULL no cover available
	u8 filtered;			// if 1, this app match the filter
	u16 playcount;			// how many time this title has bin executed
	u8 priority;			// Vote !
	u8 hidden;				// if 1, this app will be not listed
	s16 pathid;
	}
s_emu;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct 
	{
	char *name;
	char *desc;
	char *path;
	char *args;
	char *version;
	char filename[32];	// boot.dol... boot.elf...blablabl
	char mount[5];		// keep track of where is located the homebrew, as we mix sd and usb...
	int type;			// 1 hb, 2 folder 
	int priority; 		// if true is listed before others
	int iosReload;
	int hidden;			// if 1, this app will be not listed
	u32 category;
	bool checked;
	bool filtered;		// if 1, this app match the filter
	}
s_app;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct 
	{
	int neek;						// if true we are under neek emulation
	int ios;						// Current ios
	int ahbprot;
	char defMount[5]; 				// this is the default mount point, used for example to store setup. If usb is availabile, it will be usb
	GRRLIB_texImg *tex[MAXTEX];		// textures used globally... are created in video.c
	char neekName[PLN_NAME_LEN]; 	// Name of current neek nand image
	
	char tempPath[64]; 				// Temp folder
	int usbtime;
	int themeReloaded;				// Signal that a new theme was reloaded
	int interactive;				// used for selecting interactive mode upon boot
	
	int useChannelCompatibleMode;	// used for selecting interactive mode upon boot
	int saveExtendedConf;
	
	char sePath[256];				// setting editor
	char wmPath[256];				// wiimod
	
	char covers[256];				// covers path
	char coversEmu[256];			// covers path emu
	
	char *titlestxt;				// this will keep titles.txt file
	
	void *bigblock;					// about 1Mb, used to store emu structure
	}
s_vars;

typedef struct 
	{
	bool ok;
	
	int line1Y;	// Y coord of title
	int line2Y;	// Y coord short desc
	int line3Y;	// Y coord info
	
	GRRLIB_texImg *bkg;
	GRRLIB_texImg *frame;
	GRRLIB_texImg *frameBack;
	GRRLIB_texImg *frameSel;
	}
s_theme;

typedef struct
	{
	u8 nand; 	 	// 0 no nand emu	1 sd nand emu	2 usb nand emu
	char nandPath[64];			// Folder of the nand
	s_channelConfig channel;
	}
s_nandbooter;

typedef struct 
	{
	bool enabled;
	u8 time;					// Time in second to wait before autorun. 0 disable autorun

	int appMode;				// hb or channel ?
	int iosReload;				// if hb, need ios reload (1 yes)
	char path[PATHMAX]; 		// Full app path with also the device
	char filename[32]; 			// file name, tipically boot.dol or boot.elf
	char args[ARGSMAX]; 		// arguments to pass

	char nandPath[64];			// Folder of the nand
	int nand;					// Nandmode
	char asciiId[8];			// id in ascii format (used for both channels and games)
	u32 neekSlot;				// uneek game slot

	s_channelConfig channel;	// id of the channel to boot
	s_gameConfig game;
	}
s_run;

typedef struct 
	{
	int nand;			// nandmode
	char nandPath[64];		// Folder of the nand
	u8 filter[CHANNELS_MAXFILTERS];	
	}
s_chnBrowser;

typedef struct 
	{
	s_run autoboot;		// Configuration for autoboot application/channel
	s_run run;			// Configuration for manual boot application/channel
	
	u8 browseMode;
	
	// appbrowser settings
	int chnPage; 			// last page
	int appPage;			// 
	int appDev;				// 0 both, 1 sd, 2 usb
	int gamePageWii;		// 
	int gamePageGC;			// 
	int gamePageEmu;
	u32 gameFilter;			// 
	u32 gameSort;			// 0 vote, 1 name, 2 playcount
	u32 gameMode;			// GM_WII, GM_DML
	u8 gameDefaultLoader;

	// channel browser settings
	s_chnBrowser chnBrowser;
	
	bool showHidden; 		// if TRUE, apps marcked hidden will be showed
	char pwd[PWDMAXLEN+1];	// Classic wii pwd RLUD12AB, up to 8 chars
	
	u8 dmlvideomode;		// Current video mode for dml
	u8 dmlVersion;
	u8 usesGestures;

	bool runHBwithForwarder;
	
	u8 emuFilter[32];		// 
	u32 appFilter;
	char appCats[APPCATS_MAX][32];	// Application ca
	u8 volume;
	u8 enableRetroarchAutocover;
	char lastRom[PATHMAX];	// filename of latest executed rom. Actually used to import retroarch snapshots
	char subpath[64];
	char submount[6];
	u8 enableTexCache;
	u8 currentDmlWad;		// 0 = unknown...
	u8 lockCurrentMode;
	
	u8 emuDols[EMUPINMAX];
	}
s_config;

/*
s_extConfig is embeded in postloader dol
*/
typedef struct 
	{
	bool use249;
	bool disableUSB;		// If true, boottime usb init is disabled
	bool disableUSBneek;	// If true, boottime usb init is disabled (under neek)
	}
s_extConfig;

extern s_grlibSettings grlibSettings;

#ifdef MAIN
	s_config config;
	s_extConfig extConfig;
	int sdStatus = -1;
	s_vars vars;
	s_theme theme;
	const char CHANNEL_FILTERS[] = "HWFECJLMNPQ ";
	const char *CHANNELS_NAMES[] = {"HStandard Channels","WWiiWare","FNintendo NES","ENeoGeo/Arcade","CCommodore64","JSuper Nintendo","LSega Master System",
									"MSega Megadrive","NNintendo64","PTurboGraFX","QTurboGraFXCD", " Other"};
#else
	extern s_config config;
	extern s_extConfig extConfig;
	extern int sdStatus;
	extern GRRLIB_bytemapFont *fonts[3];
	extern s_vars vars;
	extern s_theme theme;
	extern const char CHANNEL_FILTERS[];
	extern const char *CHANNELS_NAMES[];
#endif

// main.c
void Subsystems (bool up);
int Initialize(int silent);
void Shutdown(void);
int MasterInterface (int full, int showCursor, int icon, const char *text, ...);	// icon 0 = none, 1 hdd, 2 hg
void ShowAboutMenu (void);
void ShowAboutPLMenu (void);
int ShowExitMenu (void);
int ShowBootmiiMenu (void);
int CheckParental (int mode);

// utils.c
u32 get_msec(bool reset);
bool FileExist (char *fn);
bool DirExist (char *path);
bool NandExits (int dev);
void mssleep (int ms);
void convertToUpperCase(char *sPtr);
char* Bin2HexAscii (void* buff, size_t insize, size_t*outsize);
int HexAscii2Bin (char *ascii, void* buff);
bool IsPngBuff (u8 *buff, int size);
void LoadTitlesTxt (void);

bool Neek2oLoadKernel (void);
bool Neek2oBoot (void);

bool ReloadPostloader (void);
bool ReloadPostloaderChannel (void);

s32 CheckDisk(void *id);

void bsort(_PTR __base, size_t __nmemb, size_t __size, int(*_compar)(const _PTR, const _PTR));
void DumpFolders (char *folder);

// dol.c
#define EXECUTE_ADDR   ((u8 *) 0x92000000)
#define BOOTER_ADDR    ((u8 *) 0x93000000)
#define ARGS_ADDR      ((u8 *) 0x93200000)
#define HBMAGIC_ADDR   ((u8 *) 0x93200000-8)
#define CMDL_ADDR      ((u8 *) 0x93200000+sizeof(struct __argv))
#define LOADER_SIZE    0x400000
#define LOADER_ADDR    ((u8 *) ARGS_ADDR - LOADER_SIZE)

u32 load_dol(const void *dolstart, struct __argv *argv);
void DolBoot (void);
int DolBootPrepare (s_run *run);
int BootWiiload (void);
bool DirectDolBoot (char *fn, char *arguments, int addpl);

// io.c
bool SetDefMount (int dev);
bool CheckForPostLoaderFolders (void);
bool MountDevices (bool silent);
bool UnmountDevices (void);

// ioConfig
bool ConfigWrite (void);
bool ConfigRead (void);
bool ExtConfigWrite (void);
bool ExtConfigRead (void);

// video.c
void Video_Init (void);
void Video_Deinit (void);
void Video_DrawBackgroud (int type);
void Video_DrawIconZ (int icon, int x, int y, f32 zx, f32 zy);
void Video_DrawIcon (int icon, int x, int y);
void Video_WaitPanel (int icon, int width, const char *text, ...); // Draw a panel with a wait screen
void Video_WaitIcon (int icon);
void Video_PanelsUpdateTime (u32 msec);
void Video_LoadTheme (int init);
void Video_DrawWIFI (void);
void Video_SetFont (int size);
void Video_SetFontMenu (int size);
void Video_DrawVersionInfo (void);
void Video_Predraw (int transparency);

// appbrowser.c
int AppBrowser (void);

// chnbrowser.c
int ChnBrowser (void);

// gamebrowser.c
int GameBrowser (void);

// emubrowser.c
int EmuBrowser (void);

// wiiloadzip
int ZipUnpack (char *path, char *target, char *dols, int *errcnt);
void WiiloadZipMenu (void);
bool WiiloadCheck (void);

// options.c

s32 menu_SwitchNandAddOption (char *menu);
s32 menu_SwitchNand (void);

// wbfsscanner
char * WBFSSCanner (bool reset);
void RunLoader(void);

// Themes
int ThemeSelect (void);

// DML/Gamecube support
void DMLResetCache (void);
int DMLSelect (void);
char * DMLScanner  (bool reset);
int DMLRun (char *folder, char *id, u32 videomode);
int DMLRunNew (char *folder, char *id, s_gameConfig *gameconf, u32 slot); //u8 videomode, u8 dmlNoDisc, u8 dmlPadHook, u8 dmlNMM);
int DMLInstall (char *gamename, size_t reqKb);
bool DEVO_Boot (char *path, u8 memcardId, bool widescreen, bool activity_led, bool wifi);

// ScreenSaver
bool LiveCheck (int reset);

// sound.c
void snd_Volume (void);
void snd_Init (void);
void snd_Stop (void);
void snd_Mp3Go (void);
void snd_Pause (void);
void snd_Resume (void);

// covercache.c
void CoverCache_Lock (void);
void CoverCache_Unlock (void);
void CoverCache_Start (void);
void CoverCache_Flush (void);
void CoverCache_Stop (void);
void CoverCache_Add (char *id, bool pt);
bool CoverCache_IsUpdated (void);
GRRLIB_texImg *CoverCache_Get (char *id);
GRRLIB_texImg *CoverCache_GetCopy (char *id);
#endif