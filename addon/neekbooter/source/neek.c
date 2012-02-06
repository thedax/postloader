#include <gccore.h> 
#include <ogc/machine/processor.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include "neek.h"

#define Debug printd

#define DI_CONFIG_SIZE         0x10
#define DI_GAMEINFO_SIZE       0x80
#define DI_GAME_NAME_OFF       0x20
#define DI_GAME_ID_OFF  	   0x0

#define CDI_CONFIG_SIZE         0x10
#define CDI_GAMEINFO_SIZE       0x100
#define CDI_GAME_TYPE_NAME_OFF  0x1C
#define CDI_GAME_NAME_OFF  		0x20
#define CDI_GAME_ID_OFF  		0x0

#define SEP 0xFF

typedef struct
{
        u32             SlotID;
        u32             Region;
        u32             Gamecount;
        u32             Config;
        u8              GameInfo[][CDI_GAMEINFO_SIZE];
} CDIConfig;

typedef struct
{
        u32             SlotID;
        u32             Region;
        u32             Gamecount;
        u32             Config;
        u8              GameInfo[][DI_GAMEINFO_SIZE];
} DIConfig;

void Debug (const char *text, ...);

void *allocate_memory(u32 size); // from triiforce

NandConfig *nandConfig = NULL;

s32 UNEEK_SelectGame( u32 SlotID )
	{
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
	char *ob = NULL, buff[64];
	
	// ob format will be <name> '\0' <id> '\0' <name> '\0' <id> '\0'....'\0'
	
	for (i = 0; i < DICfg->Gamecount; i++)
		{
		p = (char*)&DICfg->GameInfo[i];
		Debug ("%02d:%02d:%s:%s", i, DICfg->Gamecount, &p[CDI_GAME_ID_OFF], &p[CDI_GAME_TYPE_NAME_OFF]);
		obsize += (strlen(&p[CDI_GAME_NAME_OFF]) + strlen (&p[CDI_GAME_ID_OFF]) + 2); // 2 is len of the two "|" "|"
		}
		
	if (obsize > 0)
		{
		ob = calloc (1, obsize+1);
		for (i = 0; i < DICfg->Gamecount; i++)
			{
			p = (char*)&DICfg->GameInfo[i];
			
			sprintf (buff, "%s%c%s%c", &p[CDI_GAME_NAME_OFF], SEP, &p[CDI_GAME_ID_OFF], SEP);
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
		Debug ("%02d:%02d:%s:%s", i, DICfg->Gamecount, &p[DI_GAME_ID_OFF], &p[DI_GAME_NAME_OFF]);
		obsize += (strlen(&p[DI_GAME_NAME_OFF]) + strlen (&p[DI_GAME_ID_OFF]) + 2); // 2 is len of the two "|" "|"
		}
		
	if (obsize > 0)
		{
		obsize += 1;
		ob = calloc (1, obsize);
		
		for (i = 0; i < DICfg->Gamecount; i++)
			{
			p = (char*)&DICfg->GameInfo[i];
			
			sprintf (buff, "%s%c%s%c", &p[DI_GAME_NAME_OFF], SEP, &p[DI_GAME_ID_OFF], SEP);
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
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandcfg.bin");
	
	s32 fd = ISFS_Open( path, ISFS_OPEN_READ);
	Debug ("neek_GetNandConfig [ISFS_Open %d]", fd);

	if (fd <= 0)
		return false;
	
	if (nandConfig)
		free (nandConfig);
	
	// Read the header
	ret = ISFS_Read(fd, &NChead, sizeof(NChead));
	Debug ("neek_GetNandConfig [IOS_Read %d]", ret);

	u32 cfgSize = (NChead.NandCnt * NANDCONFIG_NANDINFO_SIZE) + NANDCONFIG_CONFIG_SIZE;
	nandConfig = allocate_memory (cfgSize);

	ISFS_Seek (fd, 0, 0);

	ret = ISFS_Read(fd, nandConfig, cfgSize);

	ISFS_Close(fd);
	
	//nandConfig->NandCnt = 3;
	
	int i;
	for (i = 0; i < nandConfig->NandCnt; i++)
		{
		Debug ("neek_GetNandConfig [%d: %s]", i, nandConfig->NandInfo[i]);
		}

	Debug ("neek_GetNandConfig [end]");
	
	ISFS_Deinitialize ();
	return true;
	}

bool neek_WriteNandConfig (void)
	{
	s32 ret;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	Debug ("neek_WriteNandConfig [begin]");
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandcfg.bin");
	
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
	

bool neek_NandConfigSelect (char *nand)	// Search and select the passed nand
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

	if (idx == -1) return FALSE;
	
	nandConfig->NandSel = idx;
	
	neek_WriteNandConfig ();
	
	Debug ("neek_NandConfigSelect [end]");
	return TRUE;
	}
	

bool neek_PLNandInfo (int mode, u32 *idx, u32 *status, u32 *lo, u32 *hi) // mode 0 = read, mode 1 = write
	{
	s32 ret;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	u32 data[4] ATTRIBUTE_ALIGN(32);
	
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
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandcfg.pl");
	
	s32 fd;
	
	if (!mode)
		fd = ISFS_Open( path, ISFS_OPEN_READ);
	else
		fd = ISFS_Open( path, ISFS_OPEN_WRITE);

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

	return true;
	}

bool neek_PLNandInfoRemove (void) // mode 0 = read, mode 1 = write
	{
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	Debug ("neek_PLNandInfoRemove [begin]");
		
	ISFS_Initialize ();
	
	sprintf (path, "/sneek/nandcfg.pl");
	ISFS_Delete (path);
	Debug ("neek_PLNandInfoRemove [end]");
	
	ISFS_Deinitialize ();

	return true;
	}
