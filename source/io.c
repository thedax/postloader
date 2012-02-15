#include <gccore.h>
#include <unistd.h>
#include <fat.h>
#include <sys/stat.h> //for mkdir 
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include "usbstorage.h"
#include "globals.h"

const DISC_INTERFACE* interface[DEVMAX] = {NULL,NULL};
static int mounted = 0;
static int lastDevTried = 0;

s32 Fat_Mount(int dev, int silent)
	{
	int ret = 0;
	time_t t, ct, lt = 0;
	int dt;
	char buff[128], buff2[32];
	char m[5];
	
	ct = time(NULL);
	if (dev == DEV_SD)
		{
		lastDevTried = DEV_SD;
		interface[dev]=(DISC_INTERFACE*)&__io_wiisd;
		strcpy (m, "sd");
		t = ct + 1;
		}
	else if (dev == DEV_USB && vars.usbtime)
		{
		lastDevTried = DEV_USB;
		
		if (vars.ios == IOS_DEFAULT)
			interface[dev]=(DISC_INTERFACE*)&__io_wiiums;
		else
			interface[dev]=(DISC_INTERFACE*)&__io_usbstorage;
		
		strcpy (m, "usb");
		
		t = ct + vars.usbtime;
		}
	else
		return -1;
	
	while (t > ct)
		{
		ct = time(NULL);
		
		dt = (int)(t-ct);
		
		if (dev == DEV_USB )
			{
			if (interactive)
				sprintf (buff, "Waiting for USB device - (B) interrupt...");
			else
				sprintf (buff, "Waiting for USB device - (A) interactive (B) interrupt...");

			if (dt <= 10)
				{
				sprintf (buff2, "\n%d seconds left", dt);
				strcat (buff, buff2);
				}
			}
		else
			strcpy (buff, "Searching for SD device");

		if (!silent) ret = MasterInterface (0, 0, 1, buff);
		
		if (ret == 1) interactive = 1;
		if (ret == 2) break;
		if (ct != lt)
			{
			if (interface[dev]->startup() &fatMountSimple(m, interface[dev]))
				{
				mounted = 1;
				strcpy (vars.mount[dev], m);
				return 1; // Mounted
				}
			lt = ct;
			}
			
		if (dev == DEV_SD) break;
		
		mssleep(10); // 10 msec
		}
	
	interface[dev]->shutdown();
	mounted = 0;
	return 0;
	}
	
void Fat_Unmount(void)
	{
	ConfigWrite ();

	int i;
	char mount[64];
	
	for (i = 0; i < DEVMAX; i++)
		{
		if (vars.mount[i][0] != '\0')
			{
			sprintf (mount, "%s:/", vars.mount[i]);
			fatUnmount(mount);
			interface[i]->shutdown();
			mounted = 0;
			vars.mount[i][0] = '\0';
			}
		}
	}

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
	strcpy (vars.defMount, vars.mount[dev]);
	if (vars.defMount != '\0') return TRUE;
	
	return FALSE;
	}

bool IsDevValid (int dev)
	{
	if (vars.mount[dev][0] != '\0') return TRUE;
	return FALSE;
	}

bool MountDevices (bool silent)
	{
	bool stopMount = false;
	char path[PATHMAX];
	bool cfgSD = FALSE, cfgUSB = FALSE;
	
	Fat_Unmount ();

	// First try to mount SD
	if (Fat_Mount(DEV_SD, silent))
		{
		// Try to load configuration file... maybe a user have choosen to use the SD as primary device for postLoader
		SetDefMount (DEV_SD);
		if (vars.neek == NEEK_NONE)
			sprintf (path, "%s://ploader.sd", vars.defMount);
		else
			sprintf (path, "%s://ploader/sdonly.nek", vars.defMount);
		
		if (fsop_FileExist (path))
			stopMount = TRUE;

		cfgSD = ConfigRead ();
		}
	
	// Contine with next device
	if (!stopMount)
		{
		if (Fat_Mount(DEV_USB, silent) && !cfgSD)
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

	return FALSE;
	}