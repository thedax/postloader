#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <ogc/machine/processor.h>
#include <ogc/ipc.h>
#include <ogc/isfs.h>
#include "debug.h"

void *allocate_memory(u32 size);

// This is the minimal required structure
typedef struct {
        u32 autoboot;
        u32 version;
        u32 ReturnTo;
} Settings;

enum {
		AUTOBOOT_DISABLED,
		AUTOBOOT_HBC,
		AUTOBOOT_BOOTMII_IOS,
		AUTOBOOT_SYS,
		AUTOBOOT_FILE,
		AUTOBOOT_ERROR
};
enum {	
		RETURNTO_SYSMENU,
		RETURNTO_PRIILOADER,
		RETURNTO_AUTOBOOT
};

#define VERSION		04

bool CreatePriiloaderSettings (char *nandpath, u8 * iniBuff, s32 iniSize)
	{
	if (iniBuff == NULL || iniSize == 0) return false;
	
	s32 fd;
	Settings *settings=(Settings *)iniBuff;
	
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	sprintf (path, "%s/title/00000001/00000002/data/loader.ini", nandpath);
	Debug ("CreatePriiloaderSettings: %s", path);

	settings->autoboot = AUTOBOOT_FILE;
	settings->ReturnTo = RETURNTO_AUTOBOOT;
	
	ISFS_CreateFile(path, 0, 3, 3, 3);
	fd = ISFS_Open(path, 1|2 );
	if( fd < 0 )
		{
		return false;
		}
	if(ISFS_Write( fd, iniBuff, iniSize )<0)
		{
		ISFS_Close( fd );
		return false;
		}
	ISFS_Close (fd);
	Debug ("CreatePriiloaderSettings: ok (%d)!", sizeof( Settings ));
	
	return true;
	}
	
// The same of above but used in real nand to configure a neek nand on fs
bool CreatePriiloaderSettingsFS (char *nandpath, u8 * iniBuff, s32 iniSize)
	{
	if (iniBuff == NULL || iniSize == 0) return false;
	
	FILE *f;
	Settings *settings=(Settings *)iniBuff;
	
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	
	sprintf (path, "%s/title/00000001/00000002/data/loader.ini", nandpath);
	Debug ("CreatePriiloaderSettings: %s", path);
	
	settings->autoboot = AUTOBOOT_FILE;
	settings->ReturnTo = RETURNTO_AUTOBOOT;

	f = fopen (path, "wb");
	if(!f)
		{
		return false;
		}
	if(fwrite( iniBuff, 1, iniSize, f) != iniSize)
		{
		fclose (f);
		return false;
		}
	fclose (f);
	Debug ("CreatePriiloaderSettings: ok (%d)!", sizeof( Settings ));
	
	return true;
	}