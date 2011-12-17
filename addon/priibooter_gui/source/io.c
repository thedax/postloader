#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <unistd.h>
#include <fat.h>
#include <sys/stat.h> //for mkdir 
#include <sdcard/wiisd_io.h>
#include <ogc/usbstorage.h>
#include "grlib/grlib.h"
#include "common.h"

extern char mex1[256];
extern char mex2[256];

const DISC_INTERFACE* interface[DEVMAX] = {NULL,NULL};
char mount[DEVMAX][16];

s32 Fat_Mount(int dev, int *keypressed)
	{
	time_t t, ct, lt = 0;
	char m[5];
	
	ct = time(NULL);
	if (dev == DEV_SD)
		{
		interface[dev]=(DISC_INTERFACE*)&__io_wiisd;
		strcpy (m, "sd");
		t = ct + 1;
		}
	else if (dev == DEV_USB)
		{
		interface[dev]=(DISC_INTERFACE*)&__io_usbstorage;
		strcpy (m, "usb");
		t = ct + 15;
		}
	else
		return -1;
	
	printf ("Mounting %s device", m);

	while (t > ct)
		{
		ct = time(NULL);
		
		if (ct != lt)
			{
			/*
			if (grlib_GetUserInput())
				{
				*keypressed = 1;
				}
			*/
			if (interface[dev]->startup() &fatMountSimple(m, interface[dev]))
				{
				strcpy (mount[dev], m);
				return 1; // Mounted
				}
			lt = ct;
			}
			
		if (dev == DEV_SD) break;
		
		if (t - ct < 10)
			{
			sprintf (mex2, "Waiting for USB device (%d)", (int)(t - ct));
			}
		fadeIn (0);
		}
	
	interface[dev]->shutdown();
	
	printf ("\n");
	return 0;
	}

// Unmount all mounted devices	
void Fat_Unmount(void)
	{
	int i;
	char mnt[64];
	
	for (i = 0; i < DEVMAX; i++)
		{
		if (mount[i][0] != '\0')
			{
			sprintf (mnt, "%s:/", mount[i]);
			fatUnmount(mnt);
			interface[i]->shutdown();
			mount[i][0] = '\0';
			}
		}
	}
