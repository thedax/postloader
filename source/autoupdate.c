#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>

#include "globals.h"
#include "ios.h"
#include "http.h"
#include "devices.h"

static int ShowInfo (char *buff, char *ver)
	{
	char *title;
	
	Video_SetFont(TTFNORM);
	
	int j = 0, i = strlen("postLoader ");
	while (buff[i] != '\r' && buff[i] != '\n')
		ver[j++] = buff[i++];
	ver[j++] = '\0';
	
	title = calloc (1, strlen (buff));

	if (strcmp (VER, ver) == 0)
		{
		//grlib_menu ("You already have the last version", "   OK   ");
		//free (buff);
		//return -1;
		
		strcpy (title, "You already have the last version!!!\n\n");
		}
	

	int k, nl, lines;
	
	//strcpy (title, "");
	j = strlen (title);
	nl = 0;
	k = 0;
	lines = 0;
	for (i = 0; i < strlen (buff); i++)
		{
		if (k > 80) nl = 1;
		
		if (buff[i] == '\n')
			{
			k = 0;
			nl = 0;
			lines ++;
			}
		if (nl && buff[i] == ' ')
			{
			strcat (title, "\n");
			k = 0;
			nl = 0;
			lines ++;
			}
		else
			{
			title[j] = buff[i];
			title[j+1] = 0;			
			}
			
		if (lines > 17)
			{
			strcat (title, "... CUT ...");
			j = strlen(title);
			break;
			}
		j++;
		k++;
		}
	title[j] = 0;

	int item = grlib_menu (title, "Update to %s~Skip", ver);
	free (title);
	free (buff);

	return item;
	}

static int cb_mode = 0;
static void cb_au (void)
	{
	static u32 mstout = 0;
	u32 ms = ticks_to_millisecs(gettime());

	if (mstout < ms)
		{
		if (cb_mode == 0)
			Video_WaitPanel (TEX_HGL, "Please wait %d %%|downloading %u of %u Kb", (http.bytes * 100)/http.size, http.bytes / 1024, http.size / 1024);

		if (cb_mode == 1)
			Video_WaitPanel (TEX_HGL, "Please wait...|updating postLoader");
			
		mstout = ms + 200;
		}
	}

int AutoUpdate (void)
	{
	char buff[300];
	char *outbuf;
	u32 outlen=0;
	
	cb_mode = 0;

	grlib_Redraw ();
	grlib_PushScreen ();
	
	Video_WaitPanel (TEX_HGL, "Checking online update");
	sprintf (buff, "http://postloader.googlecode.com/svn/trunk/historii.txt");
	outbuf = (char*)downloadfile (buff, &outlen, NULL); // outbuf is 0 terminated
	if (!outbuf) 
		{
		Video_WaitPanel (TEX_HGL, "Error getting update informations (%d)", outlen);
		sleep (3);
		return 0;
		}
		
	char ver[64];
	if (ShowInfo (outbuf,ver) == 0)
		{
		sprintf (buff, "http://postloader.googlecode.com/files/plupdate.%s.zip", ver);
		outbuf = (char*)downloadfile (buff, &outlen, cb_au); // outbuf is 0 terminated
		
		Debug ("AutoUpdate %s -> %d", buff, outlen);
		int fail = false;
		
		if (!outbuf) fail = true;
		if (outlen > 2 && !(outbuf[0] == 'P' && outbuf[1] == 'K')) fail = true;
		if (outlen < 2) fail = true;
		if (outlen != http.size) fail = true; // wow !
		
		if (fail)		
			{
			if (outbuf) free (outbuf);
			grlib_menu ("The update file doesn't exist. Please retry later", "   OK   ");
			return 0;
			}
		
		sprintf (buff, "%s://ploader/temp/update.zip", vars.defMount);
		FILE *f = fopen (buff, "wb");
		outlen = fwrite (outbuf, 1, outlen, f);
		fclose (f);
		if (outlen != http.size) 
			{
			grlib_menu ("An error is occured while writing to device", "   OK   ");
			unlink (buff);
			return 0;
			}

		char target[128];
		sprintf (target, "%s://ploader/temp/update/", vars.defMount);
		
		fsop_CreateFolderTree (target);
		
		int errcnt;
		ZipUnpack (buff, target, NULL, &errcnt);
		
		if (errcnt)
			{
			sprintf (buff, "An error is occured during unpacking of updatefile (%d)", errcnt);
			grlib_menu (buff, "   OK   ");
			free (outbuf);
			return 0;
			}
		unlink (buff);
		
		free (outbuf);
		
		// We can update, no errors
		sprintf (buff, "%s://ploader/temp/update/", vars.defMount);
		
		cb_mode = 1;
		
		if (devices_Get(DEV_SD))
			{
			sprintf (target, "%s://apps/postloader/boot.dol", devices_Get(DEV_SD));
			if (fsop_FileExist (target))
				{
				sprintf (target, "%s://", devices_Get(DEV_SD));
				fsop_CopyFolder (buff, target, cb_au);
				}
			}
		if (devices_Get (DEV_USB))
			{
			sprintf (target, "%s://apps/postloader/boot.dol", devices_Get(DEV_USB));
			if (fsop_FileExist (target))
				{
				sprintf (target, "%s://", devices_Get(DEV_USB));
				fsop_CopyFolder (buff, target, cb_au);
				}
			}
		
		if (grlib_menu ("postLoader should be restarted now, proceed ?", "   Yes   ##1~No, after##-1") == 1)
			ReloadPostloader();
		
		}

	return 1;
	}
