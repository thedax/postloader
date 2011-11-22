/*-------------------------------------------------------------

uneek_fs.c -- USB mass storage support, using a modified uneek fs-usb

rev. 1.00		first draft
rev. 1.01		second draft
added detection and support for SDHC access in SNEEK
added check_uneek_fs_type function to see if it's sneek or uneek
rev. 1.02		third draft
added the UNEEK_FS_BOTH option to the init_uneek_fs function.
when the parameter is set, both sd and usb access are rerouted to the sd card.
it can only be used with sneek and could serve programs like Joyflow on a sd only
setup.
the exit_uneek_fs function needs to be called when that setup is used.
since I can't disable usb, not knowing if sd access is still needed, and
I can't disable sd if usb access if sd is still needed.
It's probably exceptional that one will be shutdown and the other still used,
but it's better to stay on the safe area.
rev. 1.04	fourth draft.
shutdown function is now a stub as some programs call it before they end to force a remount
exit_uneek_fs added to properly shutdown the uneek_usb_fs file system.
max_write_sectors increased to speedup things. Transfer speed from wiixplorer gone up 
from 20KB/s to 265KB/s.  

Copyright (C) 2011 Obcd

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <locale.h>
#include <malloc.h>
#include <ogc/isfs.h>
//#include <sdcard/wiisd_io.h>
//#include <gctypes.h>
//#include <ogc/disc_io.h>

#define MAX_READ_SECTORS			16			//yet to be determined how much is allowed?
#define MAX_WRITE_SECTORS			16			//yet to be determined how much is allowed?
#define CACHE_SECTOR_LOCATION		1			//not sure if it will improve speed much...

#define	UNEEK_FS_NONE				0
#define UNEEK_FS_USB				1
#define UNEEK_FS_SD					2
#define UNEEK_FS_BOTH				4

#define ISFS_OPEN_ALL				64

#define DEVICE_TYPE_WII_SD (('W'<<24)|('I'<<16)|('S'<<8)|'D')


//Not yet implemented as most disk io buffers don't seem have proper alignment
//It would require a more serious modification in the code (aligning those buffers)
//#define USE_ORIGINAL_ALIGNED_BUF	0			//if the buffer is aligned already...why not?



//This one triggers the unique functions in the uneek fs
//Don't try to change it.
//If you really need to access that file with ISFS, access it in read/write mode
//As only read mode will trigger uneek fs

static char _dev_obcd[] ATTRIBUTE_ALIGN(32) = "/sneek/obcd.txt";

static s32 fu  = -1;
// static bool uneek_fs_ok = false;
static s32 uneek_fs_type = UNEEK_FS_NONE;

#ifdef CACHE_SECTOR_LOCATION
static u32 seek_cache;
#endif

// DISC_INTERFACE __io_usbstorage;
DISC_INTERFACE __io_wiisd;
extern DISC_INTERFACE __io_usbstorage;
//extern DISC_INTERFACE __io_usb2storage;

// DISC_INTERFACE methods

static bool __io_uns_IsInserted(void)
{
	return true;
}

static bool __io_uns_Startup(void)
{
	return true;
}

int usb_verbose = 0;

bool __io_uns_ReadSectors(u32 sector, u32 count, void *buffer)
{

	u8* buf;
	s32 whence;
	u32 done;
	u32 sec;
	u32 amount;
	u8* resultbuf;

	done = 0;
	sec = sector;
	if (count <= MAX_READ_SECTORS)
	{
		amount = count;
	}
	else
	{
		amount = MAX_READ_SECTORS;
	}
//	buf = (u8 *)memalign(32, 512 * amount);
//  sdhc aligns it's buffers on 64 byte boundaries
	buf = (u8 *)memalign(64, 512 * amount);

	while(done < count)
	{	
		whence = 0;
		if ((sec & 0x80000000)!= 0)
		{
			sec &= 0x7fffffff;
			whence = 1;
		}
#ifdef	CACHE_SECTOR_LOCATION
		if ((sector + done) != seek_cache)
		{  
#endif			
			ISFS_Seek(fu,sec,whence);
#ifdef CACHE_SECTOR_LOCATION		
			seek_cache = sector + done; 
		}
#endif		
		s32 ret = ISFS_Read(fu,buf,512*amount);
		if (ret == (s32)(512*amount))
		{
			resultbuf = (u8*)buffer + (done * 512);
			memcpy(resultbuf,buf,512*amount);
	 		done+=amount;
			sec = sector + done;
#ifdef CACHE_SECTOR_LOCATION		
			seek_cache = sector + done; 
#endif		
			if((count-done)<=MAX_READ_SECTORS)
			{
				amount = count - done;
			}
			else
			{
				amount = MAX_READ_SECTORS;
			}
		}
		else
		{
#ifdef CACHE_SECTOR_LOCATION		
			seek_cache = 0xfffffff8; 
#endif		
			return false;
		} 
	}
	free(buf);
	if (usb_verbose) {
		printf("usb-r: %x [%d]\n", sector, count);
		//sleep(1);
	}
	//printf("ret = %x\n",ret);
	return true;
}

bool __io_uns_WriteSectors(u32 sector, u32 count, void *buffer)
{

	u8* buf;
	s32 whence;
	u32 done;
	u32 sec;
	u32 amount;
	u8* resultbuf;

	done = 0;
	sec = sector;
	if (count <= MAX_WRITE_SECTORS)
	{
		amount = count;
	}
	else
	{
		amount = MAX_WRITE_SECTORS;
	}
//	buf = (u8 *)memalign(32, 512 * amount);
//  sdhc aligns it's buffers on 64 byte boundaries
	buf = (u8 *)memalign(64, 512 * amount);

	while(done < count)
	{	
		whence = 0;
		if ((sec & 0x80000000)!= 0)
		{
			sec &= 0x7fffffff;
			whence = 1;
		}
#ifdef	CACHE_SECTOR_LOCATION
		if ((sector + done) != seek_cache)
		{  
#endif			
			ISFS_Seek(fu,sec,whence);
#ifdef CACHE_SECTOR_LOCATION		
			seek_cache = sector + done; 
		}
#endif		
		resultbuf = (u8*)buffer + (done * 512);
		memcpy(buf,resultbuf,512*amount);
		s32 ret = ISFS_Write(fu,buf,512*amount);
	 	if (ret == (s32)(512*amount))
		{
	 		done+=amount;
			sec = sector + done;
#ifdef CACHE_SECTOR_LOCATION		
			seek_cache = sector + done; 
#endif		
			if((count-done)<=MAX_WRITE_SECTORS)
			{
				amount = count - done;
			}
			else
			{
				amount = MAX_WRITE_SECTORS;
			}
		}
		else
		{
#ifdef CACHE_SECTOR_LOCATION		
			seek_cache = 0xfffffff8; 
#endif		
			return false;
		} 
	 }
	
	free(buf);
	if (usb_verbose) {
		printf("usb-r: %x [%d]\n", sector, count);
		//sleep(1);
	}
	return true;
}

static bool __io_uns_ClearStatus(void)
{
	return true;
}

static bool __io_uns_Shutdown(void)
{

	return true;
/*
	if ((uneek_fs_type != UNEEK_FS_NONE)&&(uneek_fs_type != UNEEK_FS_BOTH))
	{
		ISFS_Close(fu);
		ISFS_Deinitialize();
		uneek_fs_type = UNEEK_FS_NONE;
	}
	// do nothing
	return true;
*/
}

static bool __io_uns_NOP(void)
{
	// do nothing
	return true;
}



bool init_uneek_fs(u32 mode)
{

	u8* buf = NULL;
	struct _fstats* pstat = NULL;
	//struct _fstats stat;

	if (uneek_fs_type == UNEEK_FS_NONE){
		ISFS_Initialize();
		fu = ISFS_Open(_dev_obcd, ISFS_OPEN_READ);
		buf = (u8*)memalign(32, sizeof(struct _fstats));
		pstat = (struct _fstats*)buf;
		pstat->file_length = 0;
		pstat->file_pos	= 0;
		if(fu >= 0)
		{
			ISFS_GetFileStats(fu,pstat);
			//printf("Filestat length returned %x\n",pstat->file_length);
			//printf("Filestat pos returned %x\n",pstat->file_pos);
		}
		if ((pstat->file_length == 0xfffffff0)&&(pstat->file_pos == 0xfffffff8)&&(fu>=0))
		{
#ifdef	CACHE_SECTOR_LOCATION		
			seek_cache = 0xfffffff8;
#endif		
			if (mode & ISFS_OPEN_WRITE)
			{
				__io_usbstorage.ioType = DEVICE_TYPE_WII_USB;
				__io_usbstorage.features = FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_USB;
				__io_usbstorage.startup = (FN_MEDIUM_STARTUP)&__io_uns_Startup;
				__io_usbstorage.isInserted = (FN_MEDIUM_ISINSERTED)&__io_uns_IsInserted;
				__io_usbstorage.readSectors = (FN_MEDIUM_READSECTORS)&__io_uns_ReadSectors;
				__io_usbstorage.writeSectors = (FN_MEDIUM_WRITESECTORS)&__io_uns_WriteSectors;
				__io_usbstorage.clearStatus = (FN_MEDIUM_CLEARSTATUS)&__io_uns_ClearStatus;
				__io_usbstorage.shutdown = (FN_MEDIUM_SHUTDOWN)&__io_uns_Shutdown;
			}
			else
			{
				__io_usbstorage.ioType = DEVICE_TYPE_WII_USB;
				__io_usbstorage.features = FEATURE_MEDIUM_CANREAD | FEATURE_WII_USB;
				__io_usbstorage.startup = (FN_MEDIUM_STARTUP)&__io_uns_Startup;
				__io_usbstorage.isInserted = (FN_MEDIUM_ISINSERTED)&__io_uns_IsInserted;
				__io_usbstorage.readSectors = (FN_MEDIUM_READSECTORS)&__io_uns_ReadSectors;
				__io_usbstorage.writeSectors = (FN_MEDIUM_WRITESECTORS)&__io_uns_NOP; //&__io_uns_WriteSectors
				__io_usbstorage.clearStatus = (FN_MEDIUM_CLEARSTATUS)&__io_uns_ClearStatus;
				__io_usbstorage.shutdown = (FN_MEDIUM_SHUTDOWN)&__io_uns_Shutdown;
			}

			uneek_fs_type = UNEEK_FS_USB;
			free (buf);
			return true;
		}
//sneek 		
		else if ((pstat->file_length == 0xfffffff1)&&(pstat->file_pos == 0xfffffff8)&&(fu>=0))
		{
#ifdef	CACHE_SECTOR_LOCATION		
			seek_cache = 0xfffffff8;
#endif		
			uneek_fs_type = UNEEK_FS_SD;
			if (mode & ISFS_OPEN_WRITE)
			{
				__io_wiisd.ioType = DEVICE_TYPE_WII_SD;
				__io_wiisd.features = FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_SD;
				__io_wiisd.startup = (FN_MEDIUM_STARTUP)&__io_uns_Startup;
				__io_wiisd.isInserted = (FN_MEDIUM_ISINSERTED)&__io_uns_IsInserted;
				__io_wiisd.readSectors = (FN_MEDIUM_READSECTORS)&__io_uns_ReadSectors;
				__io_wiisd.writeSectors = (FN_MEDIUM_WRITESECTORS)&__io_uns_WriteSectors;
				__io_wiisd.clearStatus = (FN_MEDIUM_CLEARSTATUS)&__io_uns_ClearStatus;
				__io_wiisd.shutdown = (FN_MEDIUM_SHUTDOWN)&__io_uns_Shutdown;
				
				//usb and sd will be treated equally
				//needed for joyflow with the sd only setup

				if (mode & ISFS_OPEN_ALL)
				{
					__io_usbstorage.ioType = DEVICE_TYPE_WII_USB;
					__io_usbstorage.features = FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_USB;
					__io_usbstorage.startup = (FN_MEDIUM_STARTUP)&__io_uns_Startup;
					__io_usbstorage.isInserted = (FN_MEDIUM_ISINSERTED)&__io_uns_IsInserted;
					__io_usbstorage.readSectors = (FN_MEDIUM_READSECTORS)&__io_uns_ReadSectors;
					__io_usbstorage.writeSectors = (FN_MEDIUM_WRITESECTORS)&__io_uns_WriteSectors;
					__io_usbstorage.clearStatus = (FN_MEDIUM_CLEARSTATUS)&__io_uns_ClearStatus;
					__io_usbstorage.shutdown = (FN_MEDIUM_SHUTDOWN)&__io_uns_Shutdown;
					uneek_fs_type = UNEEK_FS_BOTH;
				}
			}
			else
			{
				__io_wiisd.ioType = DEVICE_TYPE_WII_SD;
				__io_wiisd.features = FEATURE_MEDIUM_CANREAD | FEATURE_WII_SD;
				__io_wiisd.startup = (FN_MEDIUM_STARTUP)&__io_uns_Startup;
				__io_wiisd.isInserted = (FN_MEDIUM_ISINSERTED)&__io_uns_IsInserted;
				__io_wiisd.readSectors = (FN_MEDIUM_READSECTORS)&__io_uns_ReadSectors;
				__io_wiisd.writeSectors = (FN_MEDIUM_WRITESECTORS)&__io_uns_NOP; //&__io_uns_WriteSectors
				__io_wiisd.clearStatus = (FN_MEDIUM_CLEARSTATUS)&__io_uns_ClearStatus;
				__io_wiisd.shutdown = (FN_MEDIUM_SHUTDOWN)&__io_uns_Shutdown;
				if(mode & ISFS_OPEN_ALL)
				{
					__io_usbstorage.ioType = DEVICE_TYPE_WII_USB;
					__io_usbstorage.features = FEATURE_MEDIUM_CANREAD | FEATURE_WII_USB;
					__io_usbstorage.startup = (FN_MEDIUM_STARTUP)&__io_uns_Startup;
					__io_usbstorage.isInserted = (FN_MEDIUM_ISINSERTED)&__io_uns_IsInserted;
					__io_usbstorage.readSectors = (FN_MEDIUM_READSECTORS)&__io_uns_ReadSectors;
					__io_usbstorage.writeSectors = (FN_MEDIUM_WRITESECTORS)&__io_uns_NOP; //&__io_uns_WriteSectors
					__io_usbstorage.clearStatus = (FN_MEDIUM_CLEARSTATUS)&__io_uns_ClearStatus;
					__io_usbstorage.shutdown = (FN_MEDIUM_SHUTDOWN)&__io_uns_Shutdown;
					uneek_fs_type = UNEEK_FS_BOTH;
				}
			}
			free (buf);
			return true;
		}
		else
		{
			/* these should be initialised right in the ogc library */
			/* so we will ignore those for the moment */
/*
			__io_usbstorage.ioType = DEVICE_TYPE_WII_USB;
			__io_usbstorage.features = FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_USB;
			__io_usbstorage.startup = (FN_MEDIUM_STARTUP)&__io_usb_Startup;
			__io_usbstorage.isInserted = (FN_MEDIUM_ISINSERTED)&__io_usb_IsInserted;
			__io_usbstorage.readSectors = (FN_MEDIUM_READSECTORS)&__io_usb_ReadSectors;
			__io_usbstorage.writeSectors = (FN_MEDIUM_WRITESECTORS)&__io_usb_WriteSectors;
			__io_usbstorage.clearStatus = (FN_MEDIUM_CLEARSTATUS)&__io_usb_ClearStatus;
			__io_usbstorage.shutdown = (FN_MEDIUM_SHUTDOWN)&__io_usb_Shutdown;
*/

			uneek_fs_type = UNEEK_FS_NONE;
			free(buf);
			ISFS_Close(fu);
			ISFS_Deinitialize();
			return false;
		}
	}
	else	//was already initialised
	{
		return true;
	}
}


/* stay compatible with previous versions */
bool check_uneek_fs(void)
{
	if(uneek_fs_type != UNEEK_FS_NONE)
	{
		return true;
	}
	else
	{
		return false;
	}
}

s32 check_uneek_fs_type(void)
{
	return uneek_fs_type;
}

bool exit_uneek_fs(void)
{
	if (uneek_fs_type != UNEEK_FS_NONE)
	{
		ISFS_Close(fu);
		ISFS_Deinitialize();
		uneek_fs_type = UNEEK_FS_NONE;
	}
	// do nothing
	return true;
}
