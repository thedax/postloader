#include <stdint.h>
#include <string.h>
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/video_types.h>
#include <dirent.h>
#include "globals.h"
#include "fsop/fsop.h"

#define SEP 0xFF

#define BC 0x0000000100000100ULL
#define MIOS 0x0000000100000101ULL

/** Base address for video registers. */
#define MEM_VIDEO_BASE (0xCC002000)
#define IOCTL_DI_DVDLowAudioBufferConfig 0xE4

#define VIDEO_MODE_NTSC 0
#define VIDEO_MODE_PAL 1
#define VIDEO_MODE_PAL60 2
#define VIDEO_MODE_NTSC480P 3
#define VIDEO_MODE_PAL480P 4

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

	if (config.dmlvideomode == DMLVIDEOMODE_NTSC)
		{
		sram->flags = sram->flags & ~(1 << 0);	// Clear bit 0 to set the video mode to NTSC
		} 
	else
		{
		sram->flags = sram->flags |  (1 << 0);	// Set bit 0 to set the video mode to PAL
		}
	
	__SYS_UnlockSram(1); // 1 -> write changes
	
	while(!__SYS_SyncSram());
	
	// TVPal528IntDf
	
	u32 *xfb;
	static GXRModeObj *rmode;
	
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

static bool GetName (int dev, char *id, char *name)
	{
	char path[128];
	
	if (dev == DEV_SD)
		sprintf (path, "%s://games/%s/game.iso", vars.mount[dev], id);
	else
		sprintf (path, "%s://ngc/%s/game.iso", vars.mount[dev], id);
		
	FILE *f;
	f = fopen(path, "rb");
	if (!f)	
		{
		*name = '\0';
		return false;
		}
	
	fseek( f, 0x20, SEEK_SET);
	fread(name, 1, 32, f);
	fclose(f);
	
	//Debug ("GetName %s -> %s", path, name);
	
	name[31] = 0;
	return true;
	}

static int VideoModeMenu (void)
	{
	int ret;
	char menu[256];

	do
		{
		*menu = '\0';
		grlib_menuAddCheckItem (menu, 100, 1 - config.dmlvideomode, "NTSC mode");
		grlib_menuAddCheckItem (menu, 101, config.dmlvideomode, "PAL 576i mode");

		ret = grlib_menu ("DML options\n\nPress (+/-) to return to game list\nPress (B) to close", menu);
		
		if (ret == -1) return 0;
		if (ret == MNUBTN_PLUS || ret == MNUBTN_MINUS) return 1;
		
		if (ret == 100)	config.dmlvideomode = DMLVIDEOMODE_NTSC;
		if (ret == 101)	config.dmlvideomode = DMLVIDEOMODE_PAL;
		}
	while (ret >= 100 || ret < 0);
	
	return 1;
	}

int DMLSelect (void)
	{
	int ret;
	int i, j;
	DIR *pdir;
	struct dirent *pent;
	char path[128];
	char menu[2048];
	char files[MAXGAMES][64];
	char buff[64];
	char name[64];
	
	if (!IsDevValid(DEV_SD)) return 0;
	
	sprintf (path, "%s://games", vars.mount[DEV_SD]);
	
	do
		{
		*menu = '\0';
		i = 0;
		j = 0;
		
		pdir=opendir(path);
		
		while ((pent=readdir(pdir)) != NULL) 
			{
			//if (strlen (pent->d_name) ==  6)
			if (strcmp (pent->d_name, ".") && strcmp (pent->d_name, ".."))
				{
				strcpy (buff, pent->d_name);
				strcpy (files[i], buff);
				
				GetName (DEV_SD, files[i], name);
				grlib_menuAddItem (menu, i, "%s", name);
				
				i++;
				j++;
			
				if (j == MAXROW)
					{
					grlib_menuAddColumn (menu);
					j = 0;
					}
				
				if (i == MAXGAMES)
					{
					break;
					}
				}
			}
			
		closedir(pdir);

		if (i == 0)
			{
			grlib_menu ("No GC Games found", "OK");
			return 0;
			}
		
		ret = grlib_menu ("Please select a GameCube Game\n\nPress (+/-) to show options\nPress (B) to close", menu);
		if (ret == -1) return 0;
		if (ret == MNUBTN_PLUS || ret == MNUBTN_MINUS)
			{
			if (!VideoModeMenu ()) break;
			}

		if (ret == 100)	config.dmlvideomode = DMLVIDEOMODE_NTSC;
		if (ret == 101)	config.dmlvideomode = DMLVIDEOMODE_PAL;
		}
	while (ret >= 100 || ret < 0);
	
	
	sprintf (path, "%s://games/boot.bin", vars.mount[DEV_SD]);
	
	FILE *f;
	f = fopen(path, "wb");
	if (!f)	return -1;
	fwrite(files[ret], 1, 6, f);
	fclose(f);
	
 	memcpy ((char *)0x80000000, files[ret], 6);
	
	ConfigWrite ();
	
	Shutdown (0);
	
	StartMIOS ();
	
	return 1;
	}
	
int DMLRun (char *id)
	{
	char path[128];

	if (!IsDevValid(DEV_SD)) return 0;
	
	if (id[3] == 'E' || id[3] == 'J' || id[3] == 'N')
		config.dmlvideomode = DMLVIDEOMODE_NTSC;
	else
		config.dmlvideomode = DMLVIDEOMODE_PAL;

	sprintf (path, "%s://games/boot.bin", vars.mount[DEV_SD]);
	
	FILE *f;
	f = fopen(path, "wb");
	if (!f)	return -1;
	fwrite(id, 1, 6, f);
	fclose(f);
	
 	memcpy ((char *)0x80000000, id, 6);
	
	ConfigWrite ();
	
	Shutdown (0);
	
	StartMIOS ();
	return 1;
	}
	
char * DMLScanner (void)
	{
	DIR *pdir;
	struct dirent *pent;
	char path[128];
	char name[32];
	char b[128];
	char *buff = malloc (8192); // Yes, we are wasting space...
	
	*buff = 0;
	
	if (!IsDevValid(DEV_SD)) return 0;
	
	sprintf (path, "%s://games", vars.mount[DEV_SD]);
	
	fsop_GetFolderKb (path, 0);
	
	Debug ("DML: scanning %s", path);
	
	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		//if (strcmp (pent->d_name, ".") && strcmp (pent->d_name, ".."))
		if (strlen (pent->d_name) ==  6)
			{
			Video_WaitPanel (TEX_HGL, "Please wait...|Searching gamecube games");
			
			bool skip = false;
			
			if (IsDevValid(DEV_USB))
				{
				char sdp[256], usbp[256];
				
				sprintf (sdp, "%s://games/%s", vars.mount[DEV_SD], pent->d_name);
				sprintf (usbp, "%s://games/%s", vars.mount[DEV_USB], pent->d_name);
				
				if (fsop_DirExist (usbp))
					{
					u32 sdkb, usbkb;
					
					sdkb = fsop_GetFolderKb (sdp, NULL);
					usbkb = fsop_GetFolderKb (usbp, NULL);
					
					if (sdkb != usbkb)
						{
						char mex[256];
						fsop_KillFolderTree (sdp, NULL);
						
						sprintf (mex, "Folder '%s' removed\n as it has the wrong size", sdp);
						grlib_menu (mex, "   OK   ");
						skip = true;
						}
					}
				}
			if (!skip)
				{
				if (!GetName (DEV_SD, pent->d_name, name)) continue;
				sprintf (b, "%s%c%s%c%d%c", name, SEP, pent->d_name, SEP, DEV_SD, SEP);
				strcat (buff, b);
				}
			}
		}
		
	closedir(pdir);

	if (IsDevValid(DEV_USB))
		{
		sprintf (path, "%s://ngc", vars.mount[DEV_USB]);
		
		Debug ("DML: scanning %s", path);
		
		pdir=opendir(path);
		
		while ((pent=readdir(pdir)) != NULL) 
			{
			//if (strcmp (pent->d_name, ".") && strcmp (pent->d_name, ".."))
			if (strlen (pent->d_name) == 6 && strstr (buff, pent->d_name) == NULL)	// Make sure to not add the game twice
				{
				Video_WaitPanel (TEX_HGL, "Please wait...|Searching gamecube games");
				if (!GetName (DEV_USB, pent->d_name, name)) continue;
				sprintf (b, "%s%c%s%c%d%c", name, SEP, pent->d_name, SEP, DEV_USB, SEP);
				strcat (buff, b);
				}
			}
			
		closedir(pdir);
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

	if (!IsDevValid(DEV_SD))
		{
		Debug ("DMLInstall (%s): ERR SD Device invalid", gamename);
		return 0;
		}
	
	sprintf (path, "%s://games", vars.mount[DEV_SD]);

	devKb = fsop_GetFreeSpaceKb (path);
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
		
		//sprintf (path, "%s://games", vars.mount[DEV_SD]);
		pdir=opendir(path);
		
		while ((pent=readdir(pdir)) != NULL) 
			{
			if (strlen (pent->d_name) ==  6)
				{
				strcpy (files[i], pent->d_name);
				
				GetName (DEV_SD, files[i], name);
				if (strlen (name) > 12)
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
			sprintf (gamepath, "%s://games/%s", vars.mount[DEV_SD], files[ret]);
			
			Debug ("DMLInstall deleting '%s'", gamepath);
			fsop_KillFolderTree (gamepath, NULL);
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
	
