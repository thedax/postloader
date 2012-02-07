#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>

#include "globals.h"
#include "ios.h"
#include "http.h"

int AutoUpdate (void)
	{
	char buff[300];
	char *outbuf;
	u32 outlen=0;
	int build, dolsize;
	char *p;
	
	grlib_Redraw ();
	
	grlib_PushScreen ();
	
	Video_WaitPanel (TEX_HGL, "Checking online update");
	sprintf (buff, "http://www.ilpistone.com/wii/download/build.txt");
	outbuf = (char*)downloadfile (buff, &outlen, NULL); // outbuf is 0 terminated
	if (!outbuf) 
		{
		Video_WaitPanel (TEX_HGL, "Error getting update informations (%d)", outlen);
		sleep (3);
		return 0;
		}
	
	build = atoi(outbuf);
	if (build <= 0)
		{
		Video_WaitPanel (TEX_HGL, "Error getting build number");
		sleep (3);
		free (outbuf);
		return 0;
		}
	
	p = strstr(outbuf, ":");
	if (!p || atoi(++p) <= 0)
		{
		Video_WaitPanel (TEX_HGL, "Error getting file size");
		sleep (3);
		free (outbuf);
		return 0;
		}
	
	dolsize = atoi(p);
	free (outbuf);
	
	if (BUILD >= build)
		{
		Video_WaitPanel (TEX_HGL, "You already have last version !");
		sleep (3);
		return 0;
		}

	Video_WaitPanel (TEX_HGL, "Downloading build %d (%d Kb)", build, dolsize/1024);
	sprintf (buff, "http://www.ilpistone.com/wii/download/postloader.dol");
	outbuf = (char*)downloadfile (buff, &outlen, NULL); // outbuf is 0 terminated
	if (!outbuf) 
		{
		Video_WaitPanel (TEX_HGL, "ERROR: Retriving update dol");
		sleep (3);
		return 0;
		}

	int updates = 0;
	if (outlen == dolsize)
		{
		//suficientes bytes
		FILE *f;
		char path[PATHMAX];
		
		if (IsDevValid (DEV_SD))
			{
			sprintf (path, "sd://postloader.dol");
			if (fsop_FileExist (path))
				{
				f = fopen (path, "wb");
				if (f)
					{
					fwrite (outbuf, outlen, 1, f);
					fclose (f);
					
					updates++;
					}
				}

			sprintf (path, "sd://apps/postloader/boot.dol");
			if (fsop_FileExist (path))
				{
				f = fopen (path, "wb");
				if (f)
					{
					fwrite (outbuf, outlen, 1, f);
					fclose (f);
					
					updates++;
					}
				}
			}

		if (IsDevValid (DEV_USB))
			{
			sprintf (path, "usb://apps/postloader/boot.dol");
			if (fsop_FileExist (path))
				{
				f = fopen (path, "wb");
				if (f)
					{
					fwrite (outbuf, outlen, 1, f);
					fclose (f);
					
					updates++;
					}
				}
			}
		}
	else
		{
		Video_WaitPanel (TEX_HGL, "ERROR: Size don't match");
		sleep (3);
		}

	if (updates)
		{
		// This is just to make update count
		sprintf (buff, "http://www.ilpistone.com/wii/download.php?id=update");
		outbuf = (char*)downloadfile (buff, &outlen, NULL); // outbuf is 0 terminated
		if (outbuf) free(outbuf);

		Video_WaitPanel (TEX_HGL, "Update ok, now postLoader will reboot wii");
		sleep (3);

		Shutdown (0);
		SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
		}
	else
		{
		Video_WaitPanel (TEX_HGL, "No valid path to update found");
		sleep (3);
		}
	
	return 1;
	}
