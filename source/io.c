#include <gccore.h>
#include <unistd.h>
#include <fat.h>
#include <sys/stat.h> //for mkdir 
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include "usbstorage.h"
#include "globals.h"
#include "devices.h"

bool CheckForPostLoaderFolders (void)
	{
	char path[PATHMAX];
	
	sprintf (path, "%s://ploader",vars.defMount);
	if (!fsop_DirExist (path))
		{
		if (!fsop_MakeFolder (path)) return FALSE;
		}
	
	sprintf (path, "%s://ploader/covers",vars.defMount);
	if (!fsop_DirExist (path))
		{
		if (!fsop_MakeFolder (path)) return FALSE;
		}
	
	sprintf (path, "%s://ploader/config",vars.defMount);
	if (!fsop_DirExist (path))
		{
		if (!fsop_MakeFolder (path)) return FALSE;
		}
	
	// If temp folder exist, clean it on sturtup
	sprintf (path, "%s://ploader/temp",vars.defMount);
	if (fsop_DirExist (path))
		{
		fsop_KillFolderTree (path, NULL);
		}
	fsop_MakeFolder(path);
			
	return TRUE;
	}

bool SetDefMount (int dev)
	{
	if (devices_Get (dev))
		{
		strcpy (vars.defMount, devices_Get (dev));
		return true;
		}
	
	return false;
	}

static int cbsilent = 0;
static int cb_Mount (void)
	{
	int ret = 0;
	char buff[128];
	
	if (vars.interactive)
		sprintf (buff, "Waiting for USB device - (B) interrupt...");
	else
		sprintf (buff, "Waiting for USB device - (A) interactive (B) interrupt...");

	if (!cbsilent) ret = MasterInterface (0, 0, 1, buff);

	if (ret == 1) vars.interactive = 1;
	if (ret == 2) return 0;	// this cause mount routine to stop.

	return 1;
	}

bool MountDevices (bool silent)
	{
	cbsilent = silent;
	
	if (vars.ios > 200)
		devices_Mount (DEVMODE_CIOSX, vars.usbtime, cb_Mount);
	else
		devices_Mount (DEVMODE_IOS, vars.usbtime, cb_Mount);
	
	bool cfgSD = false, cfgUSB = false;
	
	// First try to mount SD
	if (devices_Get (DEV_SD))
		{
		// Try to load configuration file... maybe a user have choosen to use the SD as primary device for postLoader
		SetDefMount (DEV_SD);
		cfgSD = ConfigRead ();
		}
	
	// Contine with next device
	if (!extConfig.disableUSB)
		{
		if (devices_Get (DEV_USB) && !cfgSD)
			{
			SetDefMount (DEV_USB);
			cfgUSB  = ConfigRead ();
			}
		}

	if (cfgSD || cfgUSB) // we have a valid device
		{
		CheckForPostLoaderFolders ();
		return TRUE;
		}
	return false;
	}

bool UnmountDevices (void)
	{
	Debug ("UnmountDevices()");
	devices_Unmount ();
	return true;
	}