#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <ntfs.h>
#include <dirent.h>
#include "usbstorage.h"

#include "globals.h"
#include "devices.h"

#define WBFSVER "WBFSDAT0001"
#define BUFFSIZE (1024*64)
#define GISIZE 0xEC
#define SEP 0xFF

static int count = 0;
static int part = 0;

bool ScanWBFS (char *ob, char *path)
	{
	DIR *pdir;
	struct dirent *pent;
	FILE* f = NULL;
	char fn[128];
	char tmp[128];
	char buff[128];
    char gi[GISIZE];
	int i;

	Debug ("ScanWBFS '%s'", path);
	
	pdir=opendir(path);
	
	Debug ("ScanWBFS:  opendir ('%s') = 0x%X", path, pdir);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		sprintf (fn, "%s/%s", path, pent->d_name);
		
		Debug ("ScanWBFS: checking '%s'", fn);
		
		if (strlen (ob) > BUFFSIZE - 128)
			{
			Debug ("ScanWBFS: too many entryes");
			break;
			}
		
		// Let's check if it is a folder
		if (strcmp (pent->d_name, ".") != 0 && strcmp (pent->d_name, "..") != 0 && fsop_DirExist (fn))
			{
			// recurse it
			ScanWBFS (ob, fn);
			}
		else if (strstr (pent->d_name, ".wbfs") || strstr (pent->d_name, ".WBFS"))
			{
			strcpy (tmp, pent->d_name);
			tmp[24] = '\0';
			Video_WaitPanel (TEX_HGL, "%d|%s", ++count, tmp);
			
			Debug ("ScanWBFS: [FN] %s", fn);
			
			f = fopen(fn, "rb");

			if (!f) 
				{
				Debug ("   ! failed to open the file");
				continue;
				}

			fseek (f, 0x200, 0);
			fread( gi, 1, GISIZE, f);
			fclose (f);
			
			int lp = strlen(ob);
			
			// Add title
			gi[0x20 + 64] = 0;		// Make sure to not oveflow
			strcpy (buff, &gi[0x20]);
			for (i = 0; i < strlen(buff);i++) if (buff[i] < 32 || i > 125) {buff[i] = 0;break;}			
			if (strlen(buff) == 0) {Debug ("   ! inconsistent name"); ob[lp] = 0; continue; }
			strcat (ob, buff);
			sprintf (buff, "%c", SEP);
			strcat (ob, buff);
			
			// Add id
			gi[0x00 + 6] = 0;		// Make sure to not oveflow
			strcpy (buff, &gi[0x0]);
			if (strlen(buff) < 6) {Debug ("   ! inconsistent id"); ob[lp] = 0; continue; }
			strcat (ob, buff);
			sprintf (buff, "%c", SEP);
			strcat (ob, buff);
			
			// Add partition
			sprintf (buff, "%d", part);
			strcat (ob, buff);
			sprintf (buff, "%c", SEP);
			strcat (ob, buff);
			}
		}
	closedir(pdir);
	
	return TRUE;
	}

char * WBFSSCanner (bool reset)
	{
	Debug ("WBFSSCanner (begin)");
	
	char path[300];
	char b[32];
	
	FILE *f;
	count = 0;
	char *ob = calloc (1, BUFFSIZE);
	
	sprintf (path, "%s://ploader/wbfs.dat", vars.defMount);
	
	if (reset == 0)
		{
		Debug ("WBFSSCanner: reading cache file '%s'", path);
		
		f = fopen (path, "rb");
		if (!f) 
			{
			Debug ("WBFSSCanner: cache file '%s' not found", path);
			reset = 1;
			}
		else
			{
			Debug ("WBFSSCanner: cache file '%s' found, checking version", path);
			
			fread (b, 1, strlen(WBFSVER), f);
			b[strlen(WBFSVER)] = 0;
			if (strcmp (b, WBFSVER) != 0)
				{
				Debug ("WBFSSCanner: version mismatch, forcing rebuild");
				reset = 1;
				}
			else
				fread (ob, 1, BUFFSIZE-1, f);
				
			fclose (f);
			
			ob[BUFFSIZE-1] = 0;
			}
		}
	
	if (reset)
		{
		Debug ("WBFSSCanner: scannning (refreshing cache)");
		int i;
		for (i = DEV_USB; i < DEV_MAX; i++)
			{
			if (devices_Get(i))
				{
				part = devices_PartitionInfo(i);
				char path[64];
				
				sprintf (path, "%s://wbfs", devices_Get(i));
				ScanWBFS (ob, path);
				}
			}

		Debug ("WBFSSCanner: writing cache file");
		f = fopen (path, "wb");
		if (f) 
			{
			fwrite (WBFSVER, 1, strlen(WBFSVER), f);
			fwrite (ob, 1, strlen(ob)+1, f);
			fclose (f);
			}
		}
	
	// Adjust the ob
	
	Debug ("WBFSSCanner: adjust ob");
	
	int i, l;
	l = strlen(ob);
	for (i = 0; i < l; i++)
		if (ob[i] == SEP)
			ob[i] = '\0';
	
	Debug ("WBFSSCanner (end)");
	
	return ob;
	}
