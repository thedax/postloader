/*******************************************************************************
 * channels.c
 *
 * based on triiforce code
 *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <dirent.h>

#include "fsop/fsop.h"
#include "isfs.h"
#include "channels.h"
#include "globals.h"
#include "bin2o.h"
#include "mystring.h"
#include "neek.h"

bool CreatePriiloaderSettingsFS (char *nandpath, u8 * iniBuff, s32 iniSize);
extern void __exception_closeall();

/* Buffer */
static const char fs[] ATTRIBUTE_ALIGN(32) = "/dev/fs";
static const char fat[] ATTRIBUTE_ALIGN(32) = "fat";
static u32 inbuf[8] ATTRIBUTE_ALIGN(32);
static int partition = 0;

static nandDevice ndevList[] = {
	{ "Disable",				0,	0x00,	0x00 },
	{ "SD/SDHC Card",			1,	0xF0,	0xF1 },
	{ "USB 2.0 Mass Storage Device",	2,	0xF2,	0xF3 },
};

void *allocate_memory(u32 size)
{
	void *temp;
	temp = memalign(32, (size+31)&(~31) );
	memset(temp, 0, (size+31)&(~31) );
	return temp;
}

s32 Nand_Mount(nandDevice *dev)
{
    s32 fd, ret;
    u32 inlen = 0;
    ioctlv *vector = NULL;
    u32 *buffer = NULL;
    int rev = IOS_GetRevision();

    /* Open FAT module */
    fd = IOS_Open(fat, 0);
    if (fd < 0)
        return fd;

    /* Prepare vector */
    if(rev >= 21 && rev < 30000)
    {
        // NOTE:
        // The official cIOSX rev21 by Waninkoko ignores the partition argument
        // and the nand is always expected to be on the 1st partition.
        // However this way earlier d2x betas having revision 21 take in
        // consideration the partition argument.
        inlen = 1;

        /* Allocate memory */
        buffer = (u32 *)memalign(32, sizeof(u32)*3);

        /* Set vector pointer */
        vector = (ioctlv *)buffer;

        buffer[0] = (u32)(buffer + 2);
        buffer[1] = sizeof(u32);
        buffer[2] = (u32)partition;
    }

    /* Mount device */
    ret = IOS_Ioctlv(fd, dev->mountCmd, inlen, 0, vector);

    /* Close FAT module */
    IOS_Close(fd);

    /* Free memory */
    if(buffer != NULL)
        free(buffer);

    return ret;
}

s32 Nand_Unmount(nandDevice *dev)
{
	s32 fd, ret;

	/* Open FAT module */
	fd = IOS_Open("fat", 0);
	if (fd < 0)
		return fd;

	/* Unmount device */
	ret = IOS_Ioctlv(fd, dev->umountCmd, 0, 0, NULL);

	/* Close FAT module */
	IOS_Close(fd);

	return ret;
}

s32 Nand_Enable(nandDevice *dev, char *path)
{
	s32 fd, ret;

	/* Open /dev/fs */
	fd = IOS_Open("/dev/fs", 0);
	if (fd < 0)
		return fd;

	memset(inbuf, 0, sizeof(inbuf));

	/* Set input buffer */
	if (IOS_GetRevision() >= 20)
	{
		// New method, fully enable full emulation
		inbuf[0] = dev->mode | 0x100;
	} else
	{
		// Old method
		inbuf[0] = dev->mode;
	}

	/* Enable NAND emulator */
	if (path && strlen(path) > 0)
		{
		// We have a path... try to pass it to
		u32 *buffer = NULL;

		int pathlen = strlen(path)+1;

		/* Allocate memory */
		buffer = (u32 *)memalign(32, (sizeof(u32)*5)+pathlen);
    
		buffer[0] = (u32)(buffer + 4);
		buffer[1] = sizeof(u32);   // actually not used by cios
		buffer[2] = (u32)(buffer + 5);
		buffer[3] = pathlen;       // actually not used by cios
		buffer[4] = inbuf[0];            
		strcpy((char*)(buffer+5), path);
		
		ret = IOS_Ioctlv(fd, 100, 2, 0, (ioctlv *)buffer); 
		
		//grlib_dosm ("pl=%d, path='%s', IOS_Ioctlv=%d", pathlen, path, ret);
		
		free (buffer);
		}
	else
		ret = IOS_Ioctl(fd, 100, inbuf, sizeof(inbuf), NULL, 0);

	/* Close /dev/fs */
	IOS_Close(fd);

	return ret;
} 

s32 Nand_Disable(void)
{
	s32 fd, ret;

	/* Open /dev/fs */
	fd = IOS_Open("/dev/fs", 0);
	if (fd < 0)
		return fd;

	/* Set input buffer */
	inbuf[0] = 0;

	/* Disable NAND emulator */
	ret = IOS_Ioctl(fd, 100, inbuf, sizeof(inbuf), NULL, 0);

	/* Close /dev/fs */
	IOS_Close(fd);

	return ret;
} 

static int mounted = 0;

s32 get_nand_device()
	{
	return mounted;
	}

s32 Enable_Emu(int selection, char *path)
	{
	Debug ("Enable_Emu(%d, '%s')", selection, path);
	
	if (mounted != 0) return -1;
	
	s32 ret;
	nandDevice *ndev = NULL;
	ndev = &ndevList[selection];
	
	partition = 0;
	do
		{
		ret = Nand_Mount(ndev);
		
		if (ret < 0) 
			partition ++;
		}
	while (partition < 4 && ret < 0);

	Debug ("Nand_Mount = %d, part %d", ret, partition);

	if (ret < 0) 
		return ret;

	ret = Nand_Enable(ndev, path);
	if (ret < 0) 
		{
		Nand_Unmount(ndev);
		return ret;
		}
		
	Debug ("Nand_Enable = %d", ret);

	mounted = selection;
	return 0;
	}

s32 Disable_Emu()
{
	if (mounted == 0) return 0;
	
	nandDevice *ndev = NULL;
	ndev = &ndevList[mounted];
	
	Nand_Disable();
	Nand_Unmount(ndev);
	
	mounted = 0;
	
	return 0;	
}	

static bool check_text(char *s) 
{
    int i = 0;
    for(i=0; i < strlen(s); i++)
    {
        if (s[i] < 32 || s[i] > 165)
		{
			s[i] = '?';
			//return false;
		}
	}  

	return true;
}

static char *get_name_from_banner_buffer(u8 *buffer)
{
	char *out;
	u32 length;
	u32 i = 0;
	while (buffer[0x21 + i*2] != 0x00)
	{
		i++;
	}
	length = i;
	out = malloc(length+12);
	if(out == NULL)
	{
		return NULL;
	}
	memset(out, 0x00, length+12);
	
	i = 0;
	while (buffer[0x21 + i*2] != 0x00)
	{
		out[i] = (char) buffer[0x21 + i*2];
		i++;
	}
	return out;
}

static int find_string_in_buffer (u8 *buff, int buffsize, char *string)
	{
	int i, l;
	
	l = strlen(string);
	for (i = 0; i < buffsize-i; i++)
		{
		if (memcmp(&buff[i], string, l) == 0)
			return i;
		}
	return -1;
	}
	
// to identify title we should look of 0x00 0x00 0x00 0x00 0x00 [Printable char]
static char *find_title_name (u8 *buff, int buffsize)
	{
	char printable[] = "1234567890QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm ";
	char pattern[] = {'\0', '\0', '\0', '\0', '\0', '\0'};
	
	int printableLen = sizeof(printable);
	int patternLen = sizeof(pattern);
	int pid = patternLen-1;

	char *outbuff;
	char tempbuff[256];
	
	char *p, *eob;
	
	int start, j;
	
	start = find_string_in_buffer (buff, buffsize, "IMET");
	if (start < 0) return NULL;
	
	p = (char*)buff+start;
	eob = (char *) ((buff+buffsize) - (patternLen+2));

	while (p < eob)
		{
		for (j = 0; j < printableLen; j++)
			{
			if (printable[j])
				pattern[pid] = printable[j];
		
			if (memcmp(p, pattern, patternLen) == 0)
				{
				int pos, err;
				
				// We have found something valid
				memset (tempbuff, 0, sizeof(tempbuff));
				
				p += patternLen-1;
				pos = 0;
				err = -10;
				
				while (p < eob)
					{
					if (*p == '\0' && *(p+1) == '\0')
						{
						err = 0;
						break;
						}
					if (*p < 32)
						{
						err = -1;
						break;
						}
					if (*(p+1) != '\0')
						{
						err = -2;
						break;
						}
					if (pos >= 255)
						{
						err = -3;
						break;
						}
					tempbuff[pos] = *p;
					p += 2;
					pos++;
					}
				if (pos > 3 && err == 0) 
					{
					outbuff = malloc (pos + 16);
					
					if (outbuff)
						strcpy (outbuff, tempbuff);
					
					return outbuff;
					}
				}
			}
		p++;
		}
	
	return NULL;
	}

char *read_name_from_banner_app(u64 titleid, char *nandmountpoint)
	{
    char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	u8 *buffer;
	size_t readed;

	if (!nandmountpoint && strlen(nandmountpoint) > 0)
		{
		u32 num;
		s32 ret;
		dirent_t *list = NULL;
		u32 cnt = 0;
		
		sprintf(path, "/title/%08x/%08x/content", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
		
		ret = getdir(path, &list, &num);
		if (ret < 0) return NULL;
		
		for (cnt=0; cnt < num; cnt++)
			{        
			if (strstr(list[cnt].name, ".app") != NULL || strstr(list[cnt].name, ".APP") != NULL) 
				{
				sprintf(path, "/title/%08x/%08x/content/%s", TITLE_UPPER(titleid), TITLE_LOWER(titleid), list[cnt].name);
	  
				buffer = readalloc_file_from_nand (path, NULL, 368, &readed);
				if (!buffer) continue;
				
				char * out = find_title_name (buffer, readed);
				free(buffer);

				if (out)
					{
					free(list);
					return out;
					}
				}
			}
		free(list);
		}
	else
		{
		DIR *pdir;
		struct dirent *pent;
		char fnpath[256];

		sprintf(path, "%s/title/%08x/%08x/content", nandmountpoint, TITLE_UPPER(titleid), TITLE_LOWER(titleid));

		pdir=opendir(path);
		while ((pent=readdir(pdir)) != NULL) 
			{
			if (strlen (pent->d_name) < 3)
				continue;
			
			sprintf (fnpath, "%s/%s", path, pent->d_name);
			//Debug ("read_name_from_banner_app: '%s'", fnpath);
			buffer = fsop_ReadFile (fnpath, 368, &readed);
			if (!buffer) continue;

			char * out = find_title_name (buffer, readed);
			free(buffer);

			if (out)
				{
				closedir (pdir);
				return out;
				}
			}
		closedir (pdir);
		}
	
	return NULL;
	}

static char *read_name_from_banner_bin(u64 titleid, char *nandmountpoint)
	{
	u8 *buffer = NULL;
	size_t readed;
	
	if (!nandmountpoint && strlen(nandmountpoint) > 0)
		{
		char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
		sprintf(path, "/title/%08x/%08x/data/banner.bin", TITLE_UPPER(titleid), TITLE_LOWER(titleid));

		buffer = readalloc_file_from_nand (path, NULL, 160, &readed);
		if (!buffer) return NULL;
		}
	else
		{
		char path[256];
		sprintf(path, "%s/title/%08x/%08x/data/banner.bin", nandmountpoint, TITLE_UPPER(titleid), TITLE_LOWER(titleid));

		buffer = fsop_ReadFile (path, 160, &readed);
		if (!buffer) return NULL;
		}

	char *out = get_name_from_banner_buffer(buffer);
	if (out == NULL)
		{
		free(buffer);
		return NULL;
		}
	
	free(buffer);

	return out;		
	}

char *get_name(u64 titleid, char *nandmountpoint)
{
	Debug ("get_name (%llu, %s)", titleid, nandmountpoint);
	
	char *temp = NULL;
	u32 low;

	low = TITLE_LOWER(titleid);

	temp = read_name_from_banner_bin(titleid, nandmountpoint);
	if (temp == NULL)
		{
		temp = read_name_from_banner_app(titleid, nandmountpoint);
		}
	
	if (temp != NULL)
	{
		check_text(temp);

		if (*(char *)&low == 'W')
		{
			return temp;
		}
		switch(low & 0xFF)
			{
			case 'E':
				strcat (temp," (NTSC-U)");
				break;
			case 'P':
				strcat (temp," (PAL)");
				break;
			case 'J':
				strcat (temp," (NTSC-J)");
				break;	
			case 'L':
				strcat (temp," (PAL)");
				break;	
			case 'N':
				strcat (temp," (NTSC-U)");
				break;		
			case 'M':
				strcat (temp," (PAL)");
				break;
			case 'K':
				strcat (temp," (NTSC)");
				break;
			default:
				break;				
			}
		}
	
	if (temp == NULL)
		{
		temp = malloc(6);
		memset(temp, 0, 6);
		memcpy(temp, (char *)(&low), 4);
		}
	
	return temp;
	}

s32 get_game_list(u64 **TitleIds, u32 *num, u8 id) // id = 0:00010001, id = 1:00010002
{
	int ret;
	u32 maxnum;
	u32 tempnum = 0;
	u32 number;
	dirent_t *list = NULL;
    char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	char folder[300];
	u32 upper;
	
	upper = 0x00010001+id;
	
	sprintf(folder, "/title/%08x", upper);
	strcpy (path, folder);
	
	ret = getdir(path, &list, &maxnum);
	if (ret < 0)
	{
		return ret;
	}
	
	if (maxnum == 0) return -1;

	u64 *temp = malloc(sizeof(u64) * maxnum);
	if (temp == NULL)
	{
		free(list);
		return -1;
	}

	int i;
	for (i = 0; i < maxnum; i++)
		{	
		if (
			// Also ignore the HBC, "JODI" and 0xaf1bf516 
			   memcmp(list[i].name, "4a4f4449", 8) != 0 && memcmp(list[i].name, "4A4F4449", 8) != 0 
			&& memcmp(list[i].name, "af1bf516", 8) != 0 && memcmp(list[i].name, "AF1BF516", 8) != 0
			&& memcmp(list[i].name, "504f5354", 8) != 0 && memcmp(list[i].name, "504F5354", 8) != 0
			// And ignore everything that's not using characters or numbers (only check 1st char)
			&& strtol(list[i].name,NULL,16) >= 0x30000000 && strtol(list[i].name,NULL,16) <= 0x7a000000 )
			{
			sprintf (path, "%s/%s/content", folder, list[i].name);
			ret = getdircount(path, &number);
			
			if (ret >= 0 && number > 1) // 1 == tmd only
				{
				temp[tempnum] = TITLE_ID(upper, strtol(list[i].name,NULL,16));
				tempnum++;
				}
			}
		}

	*TitleIds = temp;
	*num = tempnum;
	free(list);
	return maxnum;
	}

s32 get_game_listEmu(u64 **TitleIds, u32 *num, u8 id, char *nandmountpoint) // id = 0:00010001, id = 1:00010002
{
    char path[300];
    char subpath[300];
	char type[32];
	DIR *pdir;
	struct dirent *pent;
	
	if (id == 0)
		strcpy (type, "00010001");
	else
		strcpy (type, "00010002");
		
	sprintf (path, "%s/title/%s", nandmountpoint, type);
	Debug ("get_game_listEmu: '%s'", path);
	
	int items = fsop_CountDirItems (path);
	Debug ("get_game_listEmu: items = %d", items);
	if (items == 0) return -1;

	u64 *temp = malloc (sizeof(u64) * items);
	if (temp == NULL) return -1;

	// Let's browse for titles
	pdir=opendir(path);
	items = 0;
	while ((pent=readdir(pdir)) != NULL) 
		{
		if (strlen (pent->d_name) != 8 ||
			ms_strstr (pent->d_name, "4A4F4449") ||
			ms_strstr (pent->d_name, "AF1BF516") ||
			ms_strstr (pent->d_name, "504F5354") // postloader
			)
			continue;
		
		sprintf (subpath, "%s/%s/content", path, pent->d_name);

		int count = fsop_CountDirItems (subpath);
		//Debug ("get_game_listEmu: subpath '%s', %d", subpath, items);
		
		if (count > 1) // 1 == tmd only
			{
			temp[items] = TITLE_ID(strtol(type,NULL,16), strtol(pent->d_name,NULL,16));
			items++;
			}
		}
	closedir (pdir);

	*TitleIds = temp;
	*num = items;
	return 1;
	}
	
#define NEEK2O_NAND "usb://Nands/pln2o"
#define NEEK2O_SNEEK "usb://sneek"

bool SetupNeek2o (void)
	{
	char path[256];
	char pathBak[256];
	char fn[64];

	sprintf (fn, "%s://ploader/n2oswitch.dol", vars.defMount);
	size_t buffsize;
	u8 *buff = fsop_ReadFile (fn, 0, &buffsize);
	if (!buff)
		return false;
	
	s32 ret;
	size_t loaderIniSize = 0;
	u8 * loaderIniBuff = NULL;
	
	// Save priiloader settings
	
	sprintf (path, "%s/title/00000001/00000002/data/main.bin", NEEK2O_NAND);
	sprintf (pathBak, "%s/title/00000001/00000002/data/main.bak", NEEK2O_NAND);

	ret = unlink (pathBak);
	Debug ("SetupNeek2o: unlink %s (%d)", pathBak, ret);
	ret = rename (path, pathBak);
	Debug ("SetupNeek2o: rename %s %s (%d)", path, pathBak, ret);
	
	sprintf (path, "%s/title/00000001/00000002/data/loader.ini", NEEK2O_NAND);
	sprintf (pathBak, "%s/title/00000001/00000002/data/loader.bak", NEEK2O_NAND);
	
	loaderIniBuff = fsop_ReadFile (path, 0, &loaderIniSize);

	if (!loaderIniBuff) return false;
	
	ret = unlink (pathBak);
	Debug ("SetupNeek2o: unlink %s (%d)", pathBak, ret);
	ret = rename (path, pathBak);
	Debug ("SetupNeek2o: rename %s %s (%d)", path, pathBak, ret);

	// Store n2oswitch
	sprintf (path, "%s/title/00000001/00000002/data/main.bin", NEEK2O_NAND);
	fsop_WriteFile (path, buff, buffsize);
	
	CreatePriiloaderSettingsFS (NEEK2O_NAND, loaderIniBuff, loaderIniSize);
	free (loaderIniBuff);

	// Configure nand cache
	neek_PrepareNandForChannel (NEEK2O_SNEEK, NEEK2O_NAND);

	return true;
	}

static void cb_filecopy (void)
	{
	static time_t lastt = 0;
	time_t t = time(NULL);
	
	if (t - lastt >= 1)
		{
		u32 mb = (u32)((fsop.multy.bytes/1000)/1000);
		u32 sizemb = (u32)((fsop.multy.size/1000)/1000);
		u32 elapsed = time(NULL) - fsop.multy.start_t;
		u32 perc = (mb * 100)/sizemb;
		
		MasterInterface (1, 0, TEX_HGL, "Please wait: %u%% done, copying %u of %u Mb (%u Kb/sec)", perc, mb, sizemb, (u32)(fsop.multy.bytes/elapsed) / 1000);
		
		lastt = t;
		
		if (grlib_GetUserInput() & WPAD_BUTTON_B)
			{
			int ret = grlib_menu ("This will interrupt the copy process... Are you sure", "Yes##0|No##-1");
			if (ret == 0) fsop.breakop = 1;
			}
		}
	}

bool RunChannelNeek2o (s_run *run)
	{
	char source[256];
	char target[256];
	char np[256];
	
	// Let's copy the title, if needed
	if (run->nand == 1)
		sprintf (np, "sd:/%s", run->nandPath);
	if (run->nand == 2)
		sprintf (np, "usb:/%s", run->nandPath);
		
	sprintf(source, "%s/title/%08x/%08x", np, TITLE_UPPER(run->channel.titleId), TITLE_LOWER(run->channel.titleId));
	sprintf(target, "%s/title/%08x/%08x", NEEK2O_NAND, TITLE_UPPER(run->channel.titleId), TITLE_LOWER(run->channel.titleId));
	
	Debug ("=======================================================");
	u32 sourceSize = (u32) fsop_GetFolderBytes (source, NULL);
	u32 targetSize = (u32) fsop_GetFolderBytes (target, NULL);
	Debug ("=======================================================");

	if (sourceSize > targetSize)
		{
		fsop_CopyFolder (source, target, cb_filecopy);
		
		sprintf(source, "%s/ticket/%08x/%08x.tik", np, TITLE_UPPER(run->channel.titleId), TITLE_LOWER(run->channel.titleId));
		sprintf(target, "%s/ticket/%08x/%08x.tik", NEEK2O_NAND, TITLE_UPPER(run->channel.titleId), TITLE_LOWER(run->channel.titleId));
		
		fsop_CopyFile (source, target, cb_filecopy);
		}
	
	// Install ...
	SetupNeek2o ();
	
	// Create configuration file for n2oswitch ...
	u32 data[4];
	data[0] = 0;
	data[1] = PLNANDSTATUS_NONE;
	data[2] = TITLE_LOWER(run->channel.titleId);
	data[3] = TITLE_UPPER(run->channel.titleId);
	sprintf (target, "%s/nandcfg.pl", NEEK2O_SNEEK);
	fsop_WriteFile (target, (u8*) data, sizeof(data));

	// Boot neek2o
	Shutdown (0);
	IOS_ReloadIOS(254);
	return true;
	}
	
void RemoveNandcfg(void)
	{
	char target[256];
	sprintf (target, "%s/nandcfg.pl", NEEK2O_SNEEK);
	unlink(target);
	}


bool RestoreChannelNeek2o (void)
	{
	neek_RestoreNandForChannel (NEEK2O_SNEEK, NEEK2O_NAND);

	s32 ret;
	char path[256];
	char pathBak[256];

	sprintf (path, "%s/title/00000001/00000002/data/loader.ini", NEEK2O_NAND);
	sprintf (pathBak, "%s/title/00000001/00000002/data/loader.bak", NEEK2O_NAND);

	ret = unlink (path);
	ret = rename (pathBak, path);

	return true;
	}
	
int BootChannel(s_run *run)
	{
	s_nandbooter nb;
	u8 *tfb = ((u8 *) 0x93200000); //ARGS_ADDR; // (u8 *) 0x90000000;

	MasterInterface (1, 0, TEX_CHIP, "Executing title...");
	
	/*
	if (run->nand == NAND_REAL || vars.neek != NEEK_NONE)
		{
		Shutdown (0);
		
		WII_Initialize();
		WII_LaunchTitle((u64)(run->channel.titleId));
		exit(0);  // Use exit() to exit a program, do not use 'return' from main()
		}
	*/
	if (vars.neek == NEEK_NONE && run->channel.bootMode == 2) //neek2o
		{
		RunChannelNeek2o (run);
		}

	// Copy the triiforce image
	memcpy(EXECUTE_ADDR, nandbooter_dol, nandbooter_dol_size);
	DCFlushRange((void *) EXECUTE_ADDR, nandbooter_dol_size);

	// Load the booter
	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	entrypoint hbboot_ep = (entrypoint) BOOTER_ADDR;

	// Ok, copy execution data to 0x90000000, nandbooter will read it
	nb.nand = run->nand;
	strcpy (nb.nandPath, run->nandPath);
	memcpy (&nb.channel, &run->channel, sizeof (s_channelConfig));
	memcpy (tfb, &nb, sizeof(s_nandbooter));
	DCFlushRange((void *) tfb, sizeof(s_nandbooter));
	
	// bootit !
	grlibSettings.doNotCall_GRRLIB_Exit = true;
	Shutdown (0);
	
	u32 level;
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(level);
	__exception_closeall();
	hbboot_ep();
	_CPU_ISR_Restore(level);

	return 0;
	}
