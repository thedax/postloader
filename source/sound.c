/*

sound.c

*/

#include <asndlib.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include "cfg.h"
#include "mystring.h"
#include "globals.h"
#include "mem2.h"
#include "devices.h"
#include "fsop/fsop.h"
#include "mp3player.h"

#define VOLUME 80
#define MAXSONGS 256
#define DATABUFFER_SIZE	(32768) // defined in mp3player.c

static char *playlist[MAXSONGS];
static int stopped = 0;
static int songs = 0;
static int currentSong = 0;
static int currentVolume = 0;

static void shuffle (void)
	{
	char *p;
	int i,j;
	
	for (i = 0; i < songs; i++)
		{
		j = rand () % songs;
		
		p = playlist[i];
		playlist[i] = playlist[j];
		playlist[j] = p;
		}
	}

void snd_Mp3Go (void)
	{
	if (!songs || stopped || MP3Player_IsPlaying()) return;
	
	Debug ("snd_Mp3Go: index is %d", currentSong);	
	Debug ("snd_Mp3Go: now playing '%s'", playlist[currentSong]);
	
	MP3Player_PlayFileDirect (playlist[currentSong]);
	
	currentSong++;
	if (currentSong >= songs)
		currentSong = 0;
	}

/*

*/

static void ScanForMp3 (char *path)
	{
	DIR *pdir;
	struct dirent *pent;

	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if (songs >= MAXSONGS) break;
		
		if (ms_strstr (pent->d_name, ".mp3"))
			{
			playlist[songs] = calloc (1, strlen(path)+strlen(pent->d_name)+2);
			sprintf (playlist[songs], "%s/%s", path, pent->d_name);
			songs ++;
			}
		}
		
	closedir (pdir);
	}

void snd_Volume (void)
	{
	if (config.volume < 1 || config.volume > 5) config.volume = 3;
	
	if (config.volume == 1) currentVolume = 25;
	if (config.volume == 2) currentVolume = 50;
	if (config.volume == 3) currentVolume = 75;
	if (config.volume == 4) currentVolume = 100;
	if (config.volume == 5) currentVolume = 125;
	
	MP3Player_Volume (currentVolume);
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
	
	memset (playlist, 0, sizeof(playlist));
	
	char path[128];
	songs = 0;
	if (devices_Get(DEV_SD))
		{
		sprintf (path, "%s://ploader/mp3", devices_Get(DEV_SD));
		ScanForMp3 (path);

		sprintf (path, "%s://mp3", devices_Get(DEV_SD));
		ScanForMp3 (path);
		}
			
	if (devices_Get(DEV_USB))
		{
		sprintf (path, "%s://ploader/mp3", devices_Get(DEV_USB));
		ScanForMp3 (path);

		sprintf (path, "%s://mp3", devices_Get(DEV_USB));
		ScanForMp3 (path);
		}
		
	shuffle();
			
	stopped = 0;
	currentSong = 0;
	snd_Volume ();
	snd_Mp3Go ();

	}
	
void snd_Stop (void)
	{
	Debug ("snd_Stop");
	
	u32 i;
	for (i = currentVolume; i > 0; i-=5)
			{
			MP3Player_Volume(i);
			usleep (20*1000);
			}
	
	MP3Player_Stop();
	
	while (MP3Player_IsPlaying())
		{
		usleep (1000*1000);
		Debug ("stopping...");
		}
	ASND_End();

	// Clear playlist
	for (i = 0; i < songs; i++)
		if (playlist[i])
			{
			free (playlist[i]);
			playlist[i] = NULL;
			}

	stopped = 1;
	songs = 0;
	}

void snd_Pause (void)
	{
	Debug ("snd_Pause");
	
	u32 i;
	for (i = currentVolume; i > 0; i-=5)
			{
			MP3Player_Volume(i);
			usleep (20*1000);
			}
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
