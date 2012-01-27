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
static FILE *mp3f = NULL;
static int stopped = 0;
static int songs = 0;


static s32 reader(void *fp,void *dat, s32 size){
	
	return fread(dat, 1, size, (FILE *) fp);

}

void snd_Mp3Go (void)
	{
	if (!songs || stopped || MP3Player_IsPlaying()) return;
	
	if (mp3playing == NULL) mp3playing = mp3;
	if (mp3playing == NULL) return;
	
	if (mp3f) fclose (mp3f);
		
	mp3f = fopen (mp3playing->value, "rb");

	Debug ("Playing '%s'", mp3playing->value);
	
	//MP3Player_PlayBuffer(mp3data, filesize, NULL);
	MP3Player_PlayFile(mp3f , reader, NULL);
	
	mp3playing = mp3playing->next;
	}

/*

*/

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
			
		cfg_Debug (mp3);
		
		mp3playing = mp3;
		snd_Mp3Go ();
		}
	}
	
void snd_Stop (void)
	{
	if (mp3f) fclose (mp3f);
	MP3Player_Stop();
	ASND_End();
	cfg_Free (mp3);
	}
