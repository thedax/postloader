/*

sound.c

*/

#include <asndlib.h>
#include <mp3player.h> 
#include <dirent.h>
#include <unistd.h>
#include "cfg.h"
#include "mystring.h"
#include "globals.h"
#include "mem2.h"
#include "fsop/fsop.h"

static s_cfg *mp3 = NULL;
static s_cfg *mp3playing = NULL;
static u8 *mp3buff = NULL;

#define MAXMP3SIZE 4*1024*1024

static int stopped = 0;
static int songs = 0;

void snd_Mp3Go (void)
	{
	if (!songs || stopped || MP3Player_IsPlaying()) return;
	
	if (mp3playing == NULL) mp3playing = mp3;
	if (mp3playing == NULL) return;
			
	Debug ("snd_Mp3Go: Loading '%s'", mp3playing->value);

	FILE *f = fopen (mp3playing->value, "rb");
	s32 size = fread (mp3buff, 1, MAXMP3SIZE, f);
	fclose (f);

	Debug ("snd_Mp3Go: Playing");
	
	MP3Player_PlayBuffer(mp3buff, size, NULL);

	mp3playing = mp3playing->next;
	}

static void ScanForMp3 (char *path)
	{
	char tag[8];
	char value[128];
	DIR *pdir;
	struct dirent *pent;

	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if (ms_strstr (pent->d_name, ".mp3"))
			{
			sprintf (value, "%s/%s", path, pent->d_name);
			sprintf (tag, "%03d", songs++);
			
			cfg_SetString (mp3, tag, value);
			}
		}
		
	closedir (pdir);
	}

/*
this function must be inited after io subsystem
*/
void snd_Init (void)
	{
	Debug ("snd_Init");
	ASND_Init(NULL);
	MP3Player_Init();
	
	stopped = 1;
	
	if (mp3buff == NULL)
		mp3buff = mem2_malloc (MAXMP3SIZE);	
		
	if (mp3buff == NULL)
		return;
	
	if (mp3 == NULL)
		{
		mp3 = cfg_Alloc (NULL, 0);
		if (!mp3) return;
		
		char path[128];
		
		if (IsDevValid(DEV_SD))
			{
			sprintf (path, "%s://ploader/mp3", vars.mount[DEV_SD]);
			ScanForMp3 (path);

			sprintf (path, "%s://mp3", vars.mount[DEV_SD]);
			ScanForMp3 (path);
			}
			
		if (IsDevValid(DEV_USB))
			{
			sprintf (path, "%s://ploader/mp3", vars.mount[DEV_USB]);
			ScanForMp3 (path);

			sprintf (path, "%s://mp3", vars.mount[DEV_USB]);
			ScanForMp3 (path);
			}
			
		stopped = 0;
		mp3playing = mp3;
		snd_Mp3Go ();
		}
	}
	
void snd_Stop (void)
	{
	Debug ("snd_Stop");
	
	MP3Player_Stop();
	
	while (MP3Player_IsPlaying())
		{
		usleep (1000*1000);
		Debug ("stopping...");
		}
	ASND_End();

	cfg_Free (mp3);
	mp3 = NULL;
	mp3playing = NULL;
	
	mem2_free (mp3buff);
	
	stopped = 1;
	songs = 0;
	}

void snd_Pause (void)
	{
	Debug ("snd_Pause");
	
	MP3Player_Stop();
	
	while (MP3Player_IsPlaying())
		{
		usleep (1000*1000);
		Debug ("stopping...");
		}
	stopped = 1;
	}

void snd_Resume (void)
	{
	Debug ("snd_Resume");
	stopped = 0;
	}
