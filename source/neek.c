#include <gccore.h> 
#include <ogc/machine/processor.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include "neek.h"
#include "fsop/fsop.h"
#include "globals.h"
#include "devices.h"
#include "isfs.h"

#define DI_CONFIG_SIZE         0x10
#define DI_GAMEINFO_SIZE       0x80
#define DI_GAME_NAME_OFF       0x20
#define DI_GAME_ID_OFF  	   0x0

//#define CDI_CONFIG_SIZE         0x10
#define CDI_CONFIG_SIZE         0x20
#define CDI_GAMEINFO_SIZE       0x140
#define CDI_GAME_TYPE_NAME_OFF  0x1C
#define CDI_GAME_NAME_OFF  		0x20
#define CDI_GAME_ID_OFF  		0x0

#define SEP 0xFF

/*
typedef struct
{
        u32             SlotID;
        u32             Region;
        u32             Gamecount;
        u32             Config;
        u8              GameInfo[][CDI_GAMEINFO_SIZE];
} CDIConfig;
*/

 typedef struct
{
    u32        SlotID;
    u32        Region;
    u32        Gamecount;
    u32        Config;
    u32        Config2;
    u32        Magic;
    u32        Padding1;
    u32        Padding2;
    u8        GameInfo[][CDI_GAMEINFO_SIZE];
} CDIConfig;

enum SNEEKConfig2
{
    DML_CHEATS                = (1<<0),
    DML_DEBUGGER            = (1<<1),
    DML_DEBUGWAIT            = (1<<2),
    DML_NMM                    = (1<<3),
    DML_NMM_DEBUG            = (1<<4),
    DML_ACTIVITY_LED        = (1<<5),
    DML_PADHOOK                = (1<<6),
    DML_BOOT_DISC            = (1<<7),
    DML_BOOT_DOL            = (1<<8),
    DML_PROG_PATCH            = (1<<9),
    DML_FORCE_WIDESCREEN    = (1<<10),
};
 
enum SNEEKConfig
{
    CONFIG_PATCH_FWRITE        = (1<<0),
    CONFIG_PATCH_MPVIDEO    = (1<<1),
    CONFIG_PATCH_VIDEO        = (1<<2),
    CONFIG_DUMP_ERROR_SKIP    = (1<<3),
    CONFIG_DEBUG_GAME        = (1<<4),
    CONFIG_DEBUG_GAME_WAIT    = (1<<5), 
    CONFIG_READ_ERROR_RETRY    = (1<<6),
    CONFIG_GAME_ERROR_SKIP    = (1<<7), 
    CONFIG_MOUNT_DISC        = (1<<8),
    CONFIG_DI_ACT_LED        = (1<<9),
    CONFIG_REV_ACT_LED        = (1<<10), 
    DEBUG_CREATE_DIP_LOG    = (1<<11),
    DEBUG_CREATE_ES_LOG        = (1<<12), 
    CONFIG_SCROLL_TITLES    = (1<<13),
};

typedef struct
{
        u32             SlotID;
        u32             Region;
        u32             Gamecount;
        u32             Config;
        u8              GameInfo[][DI_GAMEINFO_SIZE];
} DIConfig;

typedef struct
{
	u64 title_id;
	u16 padding;
	u16 uid;

}__attribute__((packed))UIDSYS;

void Debug (const char *text, ...);

void *allocate_memory(u32 size); // from triiforce

NandConfig *nandConfig = NULL;

s32 neek_SelectGame( u32 SlotID )
	{
	Debug ("neek_SelectGame: %u", SlotID);
	
	s32 fd = IOS_Open("/dev/di", 0 );
	
	if( fd < 0 ) 
		{
		Debug("UNEEK_SelectGame: Failed to open /dev/di: %", fd );
		return fd;
		}
	
	u32 *vec = (u32*)memalign( 32, sizeof(u32) * 1 );
	vec[0] = SlotID;

	s32 r = IOS_Ioctl( fd, 0x23, vec, sizeof(u32) * 1, NULL, 0 );

	IOS_Close( fd );

	free( vec );

	return r;
	}
	
u32 UNEEK_GetGameCount ( void )
{
	u32 cnt ATTRIBUTE_ALIGN(32);
	s32 fd = IOS_Open("/dev/di", 0 );
	if( fd < 0 )
		return fd;

	//s32 rp = 
	IOS_Ioctl( fd, 0x24, NULL, 0, &cnt, sizeof(u32) );

	IOS_Close( fd );
	return cnt;
} 

int neek_ReadAndSelectCDIGame(char *gameid)
	{
	s32 ret;
	CDIConfig *DICfg;
	FILE *f;
	CDIConfig DIChead ATTRIBUTE_ALIGN(32);
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	int gamefound = 0;
	
	Debug ("neek_ReadAndSelectCDIGames [begin]");
		
	sprintf (path, "usb:/sneek/diconfig.bin");
	
	f = fopen (path, "r+b");
	if (!f) 
		{
		Debug ("neek_ReadAndSelectCDIGames [fopen '%s' failed]", path);
		return 0;
		}
	
	// Read the header
	ret = fread (&DIChead, 1, CDI_CONFIG_SIZE, f);
	if (ret != CDI_CONFIG_SIZE)
		{
		Debug ("neek_ReadAndSelectCDIGames [fread %d -> %d]", CDI_CONFIG_SIZE, ret);
		fclose (f);
		return 0;
		}
	
	Debug ("neek_ReadAndSelectCDIGames [DICfg->Gamecount %u]", DIChead.Gamecount);
	u32 cfgSize = (DIChead.Gamecount * CDI_GAMEINFO_SIZE) + CDI_CONFIG_SIZE;

	Debug ("neek_ReadAndSelectCDIGames [buffer size %u]", cfgSize);
	DICfg = (CDIConfig*) allocate_memory(cfgSize);

	fseek (f, 0, SEEK_SET);
	ret = fread (DICfg, 1, cfgSize, f);
	if (ret != cfgSize)
		{
		Debug ("neek_ReadAndSelectCDIGames [fread %d -> %d]", cfgSize, ret);
		fclose (f);
		return 0;
		}
		
	if (gameid) // Preselect requested game
		{
		char *p;
		int i;
		
		for (i = 0; i < DICfg->Gamecount; i++)
			{
			p = (char*)&DICfg->GameInfo[i];
			
			if (strncmp (p, gameid, 6) == 0)
				{
				Debug ("Requested gameid was found (slot %d:%s)", i, gameid);
				DICfg->SlotID = i;
				gamefound = 1;
				break;
				}
			}
		}
	
	fseek (f, 0, SEEK_SET);
	ret = fwrite (DICfg, 1, cfgSize, f);
	if (ret != cfgSize)
		{
		Debug ("neek_ReadAndSelectCDIGames [fwrite %d -> %d]", cfgSize, ret);
		fclose (f);
		return 0;
		}

	return gamefound;	
	}


char* neek_GetCDIGames(void)
	{
	s32 ret;
	CDIConfig *DICfg;
	CDIConfig DIChead ATTRIBUTE_ALIGN(32);
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	Debug ("neek_GetCDIConfig [begin]");
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/diconfig.bin");
	
	s32 fd = ISFS_Open( path, ISFS_OPEN_READ);
	Debug ("neek_GetCDIConfig [ISFS_Open %d]", fd);

	if (fd <= 0)
		return 0;
	
	// Read the header
	ret = ISFS_Read(fd, &DIChead, CDI_CONFIG_SIZE);
	Debug ("neek_GetCDIConfig [IOS_Read %d]", ret);
	if (ret != CDI_CONFIG_SIZE)
		return NULL;
	
	Debug ("neek_GetCDIConfig [DICfg->Gamecount %u]", DIChead.Gamecount);
	u32 cfgSize = (DIChead.Gamecount * CDI_GAMEINFO_SIZE) + CDI_CONFIG_SIZE;

	Debug ("neek_GetCDIConfig [buffer size %u]", cfgSize);
	DICfg = (CDIConfig*) allocate_memory(cfgSize);

	ISFS_Seek (fd, 0, 0);
	ret = ISFS_Read (fd, DICfg, cfgSize);
	Debug ("neek_GetCDIConfig [DICfg->Gamecount %u]", DICfg->Gamecount);
	ISFS_Close(fd);

	int i;
	char *p;
	
	int obsize = 0;
	char *ob = NULL, buff[128];
	
	// ob format will be <name> '\0' <id> '\0' <name> '\0' <id> '\0'....'\0'
	
	for (i = 0; i < DICfg->Gamecount; i++)
		{
		p = (char*)&DICfg->GameInfo[i];
		
		//Debug_hexdump (p, CDI_GAMEINFO_SIZE);
		//Debug ("----------------------------------------------");
		
		// Fix for corrupted diconfig.bin
		p[CDI_GAME_ID_OFF + 6] = '\0';
		p[CDI_GAME_ID_OFF + 67] = '\0';
		
		Debug ("%02d:%02d:%s:%s", i, DICfg->Gamecount, &p[CDI_GAME_ID_OFF], &p[CDI_GAME_NAME_OFF]);
		sprintf (buff, "%s%c%s%c%d%c", &p[CDI_GAME_NAME_OFF], SEP, &p[CDI_GAME_ID_OFF], SEP, i, SEP);
		obsize += strlen(buff);
		}
		
	if (obsize > 0)
		{
		ob = calloc (1, obsize+1);
		for (i = 0; i < DICfg->Gamecount; i++)
			{
			p = (char*)&DICfg->GameInfo[i];
			
			if (p[CDI_GAME_NAME_OFF-1] == '=') continue;
			
			//Debug ("> %s:%s", &p[CDI_GAME_NAME_OFF], &p[CDI_GAME_ID_OFF]);

			sprintf (buff, "%s%c%s%c%d%c", &p[CDI_GAME_NAME_OFF], SEP, &p[CDI_GAME_ID_OFF], SEP, i, SEP);
			strcat (ob, buff);
			}
		
		for (i = 0; i < obsize; i++)
			{
			if (ob[i] == SEP) ob[i] = '\0';
			}
		}
	
	Debug ("neek_GetCDIConfig [free]");

	free( DICfg );
	
	Debug ("neek_GetCDIConfig [end]");
	
	ISFS_Deinitialize ();
	return ob;
	}
	
char* neek_GetDIGames(void)
	{
	s32 ret;
	DIConfig *DICfg;
	DIConfig DIChead ATTRIBUTE_ALIGN(32);
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	Debug ("neek_GetDIConfig [begin]");
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/diconfig.bin");
	
	s32 fd = ISFS_Open( path, ISFS_OPEN_READ);
	Debug ("neek_GetDIConfig [ISFS_Open %d]", fd);

	if (fd <= 0)
		return 0;
	
	// Read the header
	ret = ISFS_Read(fd, &DIChead, DI_CONFIG_SIZE);
	Debug ("neek_GetDIConfig [IOS_Read %d]", ret);
	
	Debug ("neek_GetDIConfig [DICfg->Gamecount %u]", DIChead.Gamecount);
	u32 cfgSize = (DIChead.Gamecount * DI_GAMEINFO_SIZE) + DI_CONFIG_SIZE;

	Debug ("neek_GetDIConfig [buffer size %u]", cfgSize);
	DICfg = (DIConfig*) allocate_memory(cfgSize);

	ISFS_Seek (fd, 0, 0);
	ret = ISFS_Read (fd, DICfg, cfgSize);
	Debug ("neek_GetDIConfig [DICfg->Gamecount %u]", DICfg->Gamecount);
	ISFS_Close(fd);

	int i;
	char *p;
	
	int obsize = 0;
	char *ob = NULL, buff[64];
	
	// ob format will be <name> '\0' <id> '\0' <name> '\0' <id> '\0'....'\0'
	
	for (i = 0; i < DICfg->Gamecount; i++)
		{
		p = (char*)&DICfg->GameInfo[i];
		// Debug ("%02d:%02d:%s:%s", i, DICfg->Gamecount, &p[DI_GAME_ID_OFF], &p[DI_GAME_NAME_OFF]);
		obsize += (strlen(&p[DI_GAME_NAME_OFF]) + strlen (&p[DI_GAME_ID_OFF]) + 2); // 2 is len of the two "|" "|"
		}
		
	if (obsize > 0)
		{
		obsize += 1;
		ob = calloc (1, obsize);
		
		for (i = 0; i < DICfg->Gamecount; i++)
			{
			p = (char*)&DICfg->GameInfo[i];
			
			sprintf (buff, "%s%c%s%c%d%c", &p[DI_GAME_NAME_OFF], SEP, &p[DI_GAME_ID_OFF], SEP, i, SEP);
			strcat (ob, buff);
			}
		
		for (i = 0; i < obsize; i++)
			{
			if (ob[i] == SEP) ob[i] = '\0';
			}
		}
	
	Debug ("neek_GetDIConfig [free]");

	free( DICfg );
	
	Debug ("neek_GetDIConfig [end]");
	
	ISFS_Deinitialize ();
	return ob;
	}
	
	
bool neek_IsNeek2o (void)
	{
#ifdef DOLPHINE
	return true;
#endif

	u32 cnt = UNEEK_GetGameCount ();
	
	if (cnt >= 0x10000) // cDI
		return true;
		
	return false;
	}


char* neek_GetGames (void)
	{
	Debug ("neek_GetGames");
	
	if (neek_IsNeek2o()) // cDI
		return neek_GetCDIGames();
		
	return neek_GetDIGames();
	}

	
void neek_KillDIConfig (void)	// Reboot needed after that call
	{
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	Debug ("neek_KillDIConfig [begin]");
		
	ISFS_Initialize ();

	sprintf (path, "/sneek/diconfig.bin");
	ISFS_Delete (path);
	
	Debug ("neek_KillDIConfig [end]");
	
	ISFS_Deinitialize ();
	}

bool neek_GetNandConfig (void)
	{
	s32 ret;
	NandConfig NChead ATTRIBUTE_ALIGN(32);
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	 
	Debug ("neek_GetNandConfig [begin]");
	 
	if (nandConfig)
	  free (nandConfig);
	 
	nandConfig = NULL;
	 
	ISFS_Initialize ();
	 
	sprintf (path, "/sneek/nandcfg.bin");
	 
	s32 fd = ISFS_Open( path, ISFS_OPEN_READ);
	Debug ("neek_GetNandConfig [ISFS_Open %d]", fd);
	if (fd <= 0)
	  {
	  Debug ("neek_GetNandConfig [no config file found, only root nand available ?] [end]");
	  nandConfig = NULL;
	  return false;
	  }
	 
	// Read the header
	ret = ISFS_Read(fd, &NChead, sizeof (NandConfig));
	Debug ("neek_GetNandConfig [IOS_Read %d]", ret);
	if (ret <= 0)
	  {
	  Debug ("neek_GetNandConfig [unable to read nandcfg.bin] [end]");
	  nandConfig = NULL;

	  ISFS_Close(fd);
	  ISFS_Deinitialize ();
	  return false;
	  }
	sleep (1);
	
	Debug ("neek_GetNandConfig NChead.NandCnt = %d, NChead.NandSel", NChead.NandCnt, NChead.NandSel);
	sleep (1);
	
	if (ret > 0)
	  {
	  u32 cfgSize = (NChead.NandCnt * NANDCONFIG_NANDINFO_SIZE) + NANDCONFIG_CONFIG_SIZE;
	  if (cfgSize > sizeof(u32)) // lower values are impossible
		{
		nandConfig = allocate_memory (cfgSize);
		ISFS_Seek (fd, 0, 0);
		ret = ISFS_Read(fd, nandConfig, cfgSize);
		}
	  }
	ISFS_Close(fd);
	 
	//nandConfig->NandCnt = 3;
	if (nandConfig)
	  {
	  int i;
	  for (i = 0; i < nandConfig->NandCnt; i++)
	   {
	   Debug ("neek_GetNandConfig [%D: %s]", i, nandConfig->NandInfo[i]);
	   }
	  }
	 
	Debug ("neek_GetNandConfig [end]");
	 
	ISFS_Deinitialize ();
	return true;
	}


bool neek_WriteNandConfig (void)
	{
	s32 ret;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	if (nandConfig == NULL)
		{
		return false;
		Debug ("neek_WriteNandConfig [nothing to do]");
		}
	
	Debug ("neek_WriteNandConfig [begin]");
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandcfg.bin");
	
	ISFS_CreateFile (path, 0, 3, 3, 3);
	s32 fd = ISFS_Open( path, ISFS_OPEN_WRITE);
	Debug ("neek_WriteNandConfig [ISFS_Open %d]", fd);

	if (fd <= 0)
		return false;
	
	// Read the header
	u32 cfgSize = (nandConfig->NandCnt * NANDCONFIG_NANDINFO_SIZE) + NANDCONFIG_CONFIG_SIZE;

	ret = ISFS_Write(fd, nandConfig, cfgSize);
	Debug ("neek_WriteNandConfig [ISFS_Write %d]", ret);
	ISFS_Close(fd);

	Debug ("neek_WriteNandConfig [end]");
	
	ISFS_Deinitialize ();
	return true;
	}

void neek_KillNandConfig (void)	// Reboot needed after that call
	{
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	Debug ("neek_KillNandConfig [begin]");
		
	ISFS_Initialize ();

	sprintf (path, "/sneek/nandcfg.bin");
	ISFS_Delete (path);
	
	Debug ("neek_KillNandConfig [end]");
	
	ISFS_Deinitialize ();
	}
	

int neek_NandConfigSelect (char *nand)	// Search and select the passed nand
	{
	int i;
	int idx = -1;
	
	Debug ("neek_NandConfigSelect [begin]");
	
	for (i = 0; i < nandConfig->NandCnt; i++)
		{
		Debug ("neek_NandConfigSelect [%s/%s]", nand, (char*)nandConfig->NandInfo[i]);
		if (strstr ((char*)nandConfig->NandInfo[i], nand))
			{
			Debug ("neek_NandConfigSelect [found %d]", i);

			idx = i;
			break;
			}
		}

	if (idx == -1) return -1;
	
	nandConfig->NandSel = idx;
	
	neek_WriteNandConfig ();
	
	Debug ("neek_NandConfigSelect [end]");
	return idx;
	}
	

bool neek_PLNandInfo (int mode, u32 *idx, u32 *status, u32 *lo, u32 *hi, u32 *back2real) // mode 0 = read, mode 1 = write
	{
	s32 ret;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	u32 data[8] ATTRIBUTE_ALIGN(32);
	
	Debug ("neek_PLNandInfo [begin]");
	
	data[0] = *idx;
	data[1] = *status;
	
	if (lo) 
		data[2] = *lo;
	else
		data[2] = 0;
		
	if (hi) 
		data[3] = *hi;
	else
		data[3] = 0;
		
	if (back2real)
		data[4] = *back2real;
	else
		data[4] = 0;

	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandcfg.pl");
	
	s32 fd;
	
	if (!mode)
		fd = ISFS_Open( path, ISFS_OPEN_READ);
	else
		{
		ISFS_CreateFile (path, 0, 3, 3, 3);
		fd = ISFS_Open( path, ISFS_OPEN_WRITE);
		}

	Debug ("neek_PLNandInfo [ISFS_Open %d]", fd);

	if (fd <= 0)
		{
		Debug ("neek_PLNandInfo [ERROR]");
		return false;
		}
	
	// write only the header
	if (!mode)
		ret = ISFS_Read(fd, data, sizeof(data));
	else
		ret = ISFS_Write(fd, data, sizeof(data));
	
	Debug ("neek_PLNandInfo [IO %d]", ret);
	ISFS_Close(fd);

	Debug ("neek_PLNandInfo [end]");
	
	ISFS_Deinitialize ();

	*idx = data[0];
	*status = data[1];

	if (lo)	*lo = data[2];
	if (hi)	*hi = data[3];
	if (back2real) *back2real = data[4];

	return true;
	}
	
bool neek_PLNandInfoKill (void) // Remove nandcfg.pl... postloader do this when it start
	{
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);

	Debug ("neek_PLNandInfoKill [begin]");
	
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandcfg.pl");
	ISFS_Delete (path);

	Debug ("neek_PLNandInfoKill [end]");
	
	ISFS_Deinitialize ();

	return true;
	}
	
// This will require obcd extensions

#define CDIINITALGAMECOUNT 1024

bool neek_CreateCDIConfigBrowse (CDIConfig *DICfg, u32 *count, char *path)
	{
	DIR *pdir;
	struct dirent *pent;
	FILE* f = NULL;
	char fn[128];
	char tmp[128];
	int offset;
	
	Debug ("neek_CreateCDIConfigBrowse: [PATH] %s", path);
	
	sprintf (fn, "%s://wbfs/", devices_Get(DEV_USB));
	offset = strlen (fn);

	pdir=opendir(path);
	while ((pent=readdir(pdir)) != NULL) 
		{
		sprintf (fn, "%s/%s", path, pent->d_name);
		
		// Let's check if it is a folder
		if (strcmp (pent->d_name, ".") != 0 && strcmp (pent->d_name, "..") != 0 && fsop_DirExist (fn))
			{
			// recurse it
			neek_CreateCDIConfigBrowse (DICfg, count, fn);
			}
		else if (strstr (pent->d_name, ".wbfs") || strstr (pent->d_name, ".WBFS"))
			{
			strcpy (tmp, pent->d_name);
			tmp[24] = '\0';
			Video_WaitPanel (TEX_HGL, "%d|%s", (*count)+1, tmp);
			
			Debug ("neek_CreateCDIConfigBrowse: [FN] %s", fn);
			
			f = fopen(fn, "rb");
			if (!f) continue;
			
			//Get file size
			fseek( f, 0x200, SEEK_SET);
			fread( DICfg->GameInfo[*count], 1, CDI_GAMEINFO_SIZE, f);
			fclose (f);
			
			// Set the flag
			memcpy (&DICfg->GameInfo[*count][0x1C], "WBFS", 4);
			
			// Add filename
			strcpy ((char*)&DICfg->GameInfo[*count][0x60], &fn[offset]);
			
			(*count)++;
			}
		}
	closedir(pdir);
	
	return TRUE;
	}
	
bool neek_CreateCDIConfig (char *gameid) // gameid must contain ID6 value
	{
	char path[128];
	u32 count = 0;
	u32 cfgSize = 0;
	
	sprintf (path, "%s://wbfs", devices_Get(DEV_USB));	
	
	// Create a (big) CDIConfig
	cfgSize = (CDIINITALGAMECOUNT * CDI_GAMEINFO_SIZE) + CDI_CONFIG_SIZE;	
	CDIConfig *DICfg = (CDIConfig*) malloc(cfgSize);
	
    // Init struct
	DICfg->SlotID = 0;
    DICfg->Region = 2;
    DICfg->Gamecount = count;
    DICfg->Config = 0;
	
	// Ok, we can scan games
    count = 0;
	neek_CreateCDIConfigBrowse (DICfg, &count, path);
	
	Debug ("neek_CreateCDIConfig: found %d games", count);
	
	if (count == 0)
		return FALSE;
	
	// Let's write diconfig.bin
    DICfg->Gamecount = count;
	cfgSize = (count * CDI_GAMEINFO_SIZE) + CDI_CONFIG_SIZE;
	
	if (gameid) // Preselect requested game
		{
		char *p;
		int i;
		
		for (i = 0; i < DICfg->Gamecount; i++)
			{
			p = (char*)&DICfg->GameInfo[i];
			
			if (strncmp (p, gameid, 6) == 0)
				{
				Debug ("Requested gameid was found (slot %d:%s)", i, gameid);
				DICfg->SlotID = i;
				break;
				}
			}
		}

	sprintf (path, "%s://sneek/diconfig.bin", devices_Get(DEV_USB));	
	FILE *f = fopen(path, "wb");
	fwrite( DICfg, 1, cfgSize, f);
	count = 0x00; // This is from a bugs of neek2o
	fwrite( &count, 1, sizeof(u32), f);
	count = 0x123456; // This is to check if it fixed
	fwrite( &count, 1, sizeof(u32), f);
	fclose (f);

	free (DICfg);
	
	return TRUE;
	}
	
/////////////////////////////////////////////////////////////////////

/*
Create a nandpath.bin pointing to the required nand
*/
bool neek_PrepareNandForChannel (char *sneekpath, char *nandpath)
	{
	char path[256];
		
	Debug ("neek_PrepareNandForChannel [begin]");
		
	sprintf (path, "%s/nandpath.bin", sneekpath);
	
	char *p = strstr (nandpath, "//");
	if (p == NULL) 
		p = path;
	else
		p ++;

	FILE *f;
	f = fopen (path, "wb");
	if (f)
		{
		fwrite (p, 1, strlen(p)+1, f); // +1, as we need also the 0
		fclose (f);
		}

	Debug ("neek_PrepareNandForChannel [end]");
	return true;
	}

/*
just remove nandpath.bin
*/
bool neek_RestoreNandForChannel (char *sneekpath)
	{
	char path[256];

	sprintf (path, "%s/nandpath.bin", sneekpath);
	int ret = unlink (path);
	
	Debug ("neek_RestoreNandForChannel: unlink of '%s'->%d", path, ret);
	
	return true;
	}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
//
// UID managment functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	

static UIDSYS *uid = NULL;
static size_t uidSize;
static int uidCount;

bool neek_UID_Read (void)
	{
	ISFS_Initialize ();

	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	strcpy(path,"/sys/uid.sys");
	
	if (uid != NULL) free(uid);
	uid = (UIDSYS *)isfs_ReadFile (path, NULL, 0, &uidSize);
	uidCount = uidSize / sizeof (UIDSYS);

	ISFS_Deinitialize ();
	return (uid != NULL);
	}
	

int neek_UID_Compact (void) // return the number of erased items
	{
	int i, j, count = 0;
	
	if (uid == NULL)
		return -1;
		
	// let's count uid items
	for (i = 0; i < uidCount; i++)
		{
		if (uid[i].title_id)
			{
			count ++;
			}
		}
	
	UIDSYS *uidnew = allocate_memory(count * sizeof (UIDSYS));

	// let's count uid items
	j = 0;
	for (i = 0; i < uidCount; i++)
		{
		if (uid[i].title_id)
			{
			memcpy (&uidnew[j], &uid[i], sizeof (UIDSYS));
			//if (uidnew[j].uid < 4096)
			uidnew[j].uid = j+1;
				
			j++;
			}
		}
		
	free (uid);
	uid = uidnew;
	
	uidCount = count;
	uidSize = count * sizeof (UIDSYS);
	
	return count;
	}
	
bool neek_UID_Write (void)
	{
	if (uid == NULL) return false;
	
	neek_UID_Compact ();
	
	ISFS_Initialize ();

	isfs_WriteFile ("/sys/uid.sys", (u8*) uid, uidSize);

	ISFS_Deinitialize ();
	return (uid != NULL);
	}
	
void neek_UID_Free (void)
	{
	if (uid != NULL) free(uid);
	}

int neek_UID_Find (u64 title_id)
	{
	int i;
	u64 tid;
	
	if (uid == NULL)
		return -1;
	
	for (i = 0; i < uidCount; i++)
		{
		if (uid[i].padding == 0)
			{
			tid = uid[i].title_id;
			}
		else
			{
			//Debug ("Hidden item");
			u16 * u1 = (u16*)&tid;
			u16 * u2 = (u16*)&uid[i].title_id;
			
			u1[0] = uid[i].padding;
			u1[1] = u2[1];
			u1[2] = u2[2];
			u1[3] = u2[3];
			}
		
		if (tid == title_id)
			{
			/*
			gprintf ("title %08x/%08x = %08x/%08x\r\n", 
				TITLE_UPPER(uid[i].title_id), TITLE_LOWER(uid[i].title_id), 
				TITLE_UPPER(title_id), TITLE_LOWER(title_id)
				);
			*/
			/*
			if (uid[i].uid >= 4096)
				return -2; // found but it is a protected item
			*/	
			return i;
			}
		}
	
	return -1;
	}

int neek_UID_Count (void)
	{
	int i, count = 0;
	
	if (uid == NULL)
		return -1;
	
	for (i = 0; i < uidCount; i++)
		{
		if (uid[i].title_id > 0 /* && uid[i].uid < 4096*/)
			count ++;
		}
	
	return count;
	}
	
int neek_UID_CountHidden (void)
	{
	int i, count = 0;
	
	if (uid == NULL)
		return -1;
	
	for (i = 0; i < uidCount; i++)
		{
		if (uid[i].title_id > 0 && uid[i].padding > 0 /* && uid[i].uid < 4096 */)
			count ++;
		}
	
	return count;
	}
	
int neek_UID_Clean (void) // return the number of erased items
	{
	int i, count = 0;
	
	if (uid == NULL)
		return -1;
	
	for (i = 0; i < uidCount; i++)
		{
		if (true) //uid[i].uid < 4096)
			{
			memset (&uid[i], 0, sizeof (UIDSYS));
			count ++;
			}
		}
	
	return count;
	}

bool neek_UID_Add (u64 title_id)
	{
	gprintf ("neek_UID_Add\r\n");
	
	if (neek_UID_Find (title_id) >= 0) return false; // It is already in uid
	
	// Let's scan for an empty slot
	
	int i, found = -1;
	
	for (i = 0; i < uidCount; i++)
		if (uid[i].title_id == 0)
			{
			found = i;
			gprintf ("neek_UID_Add found = %d\r\n", found);
			break;
			}
	
	if (found == -1) // we have not found a free block, resize the uid buffer
		{
		gprintf ("neek_UID_Add notfound, growing array\r\n");
		found = uidCount;
	
		uidCount ++;
		uidSize = uidCount * sizeof (UIDSYS);
		
		UIDSYS *uidnew = allocate_memory(uidSize * sizeof (UIDSYS));
		if (uidnew == NULL)
			{
			// uid is corrupted :(
			return false;
			}
			
		memcpy (uidnew, uid, (uidCount - 1) * sizeof (UIDSYS));
		free (uid);
		uid = uidnew;
		
		found = uidCount - 1;
		}
	
	memset (&uid[found], 0, sizeof (UIDSYS));
	uid[found].title_id = title_id;
	uid[found].uid = 0; // need to be updated when saving ;)
	gprintf ("neek_UID_Add slot %d:%08X/%08X:%u \r\n", found, TITLE_UPPER (uid[found].title_id), TITLE_LOWER (uid[found].title_id), uid[found].uid);
	return true;
	}
	
bool neek_UID_Remove (u64 title_id)
	{
	int pos = neek_UID_Find (title_id);
	if (pos < 0) return false;
	
	// Simply set the item to 0....
	memset (&uid[pos], 0, sizeof (UIDSYS));
	
	return true;
	}
	
bool neek_UID_IsHidden (u64 title_id)
	{
	int pos = neek_UID_Find (title_id);
	if (pos < 0) return false;
	
	u16 *u = (u16*)&uid[pos].title_id;

	if (u[0] == 0 && uid[pos].padding != 0) return true; // It isn't hidden
	
	return false;
	}

bool neek_UID_Show (u64 title_id)
	{
	int pos = neek_UID_Find (title_id);
	if (pos < 0) return false;
	if (uid[pos].padding == 0) return false; // It isn't hidden
	
	u16 *u = (u16*)&uid[pos].title_id;
	
	// restore titleid first word from padding
	u[0] = uid[pos].padding;
	
	// clear the padding
	uid[pos].padding = 0;
	
	// now Channel should be visible
	
	return true;
	}

bool neek_UID_Hide (u64 title_id)
	{
	Debug ("neek_UID_Hide");
	int pos = neek_UID_Find (title_id);
	Debug ("neek_UID_Hide: %d, %u", pos, uid[pos].padding);
	if (pos < 0) return false;
	if (uid[pos].padding != 0) return false; // It is already hidden
	
	u16 *u = (u16*)&uid[pos].title_id;
	
	// we use the padding to store the first word of titleid
	uid[pos].padding = u[0];
	
	// now clear the first word of titleid
	u[0] = 0;
	
	// now Channel should be hidden
	
	return true;
	}

bool neek_UID_Dump (void)
	{
	int i;
	
	for (i = 0; i < uidCount; i++)
		{
		if (uid[i].uid)
			Debug ("UID: %d -> %08X/%08X:%u:%u", i, TITLE_UPPER (uid[i].title_id), TITLE_LOWER (uid[i].title_id), uid[i].uid, uid[i].padding);
		}
	
	return true;
	}
