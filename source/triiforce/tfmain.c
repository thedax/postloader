/*******************************************************************************
 * main.c
 *
 * Copyright (c) 2009 The Lemon Man
 * Copyright (c) 2009 Nicksasa
 * Copyright (c) 2009 WiiPower
 *
 * Distributed under the terms of the GNU General Public License (v2)
 * See http://www.gnu.org/licenses/gpl-2.0.txt for more info.
 *
 * Description:
 * -----------
 *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <gccore.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>

#include "tfmain.h"
#include "tools.h"
#include "isfs.h"
#include "name.h"
#include "lz77.h"
#include "nand.h"

#include <ogc/machine/processor.h>
#include "../globals.h"
#include "../ios.h"
#include "bin2o.h"

extern s32 __IOS_ShutdownSubsystems();
extern void __exception_closeall();

u8 Video_Mode;

void*	dolchunkoffset[64];			//TODO: variable size
u32		dolchunksize[64];			//TODO: variable size
u32		dolchunkcount;

void _unstub_start();

// Prevent IOS36 loading at startup
s32 __IOS_LoadStartupIOS()
{
	return 0;
}

typedef struct _dolheader
{
	u32 text_pos[7];
	u32 data_pos[11];
	u32 text_start[7];
	u32 data_start[11];
	u32 text_size[7];
	u32 data_size[11];
	u32 bss_start;
	u32 bss_size;
	u32 entry_point;
} dolheader;

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
			// And ignore everything that's not using characters or numbers(only check 1st char)
			&& strtol(list[i].name,NULL,16) >= 0x30000000 && strtol(list[i].name,NULL,16) <= 0x7a000000 )
			{
			sprintf (path, "%s/%s/content", folder, list[i].name);
			ret = getdircount(path, &number);
			
			//grlib_dosm ("%d - %s/%s - %u", ret, path, list[i].name, number);
			
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


s32 check_dol(u64 titleid, char *out, u16 bootcontent)
{
    s32 ret;
	u32 num;
	dirent_t *list;
    char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
    int cnt = 0;
	
	u8 *decompressed = NULL;
	u32 decomp_size = 0;
	
	u8 *buffer = allocate_memory(32);	// Needs to be aligned because it's used for nand access
	
	if (buffer == NULL)
	{
		return -1;
	}
 
    u8 check[6] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
 
    sprintf(path, "/title/%08x/%08x/content", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
    ret = getdir(path, &list, &num);
    if (ret < 0)
	{
		free(buffer);
		return ret;
	}
	
	for (cnt=0; cnt < num; cnt++)
    {        
        if ((strstr(list[cnt].name, ".app") != NULL || strstr(list[cnt].name, ".APP") != NULL) && (strtol(list[cnt].name, NULL, 16) != bootcontent))
        {			
			memset(buffer, 0, 32);
            sprintf(path, "/title/%08x/%08x/content/%s", TITLE_UPPER(titleid), TITLE_LOWER(titleid), list[cnt].name);
  
            ret = read_file_from_nand(path, buffer, 32);
	        if (ret < 0)
	        {
	    	    // Error is printed in read_file_from_nand already
				continue;
	        }

			if (isLZ77compressed(buffer))
			{
				//Print("Found LZ77 compressed content --> %s\n", list[cnt].name);
				//Print("This is most likely the main DOL, decompressing for checking\n");

				// We only need 6 bytes...
				ret = decompressLZ77content(buffer, 32, &decompressed, &decomp_size, 32);
				if (ret < 0)
				{
					free(list);
					free(buffer);
					return ret;
				}				
				memcpy(buffer, decompressed, 8);

				free(decompressed);
 			}
			
	        ret = memcmp(buffer, check, 6);
            if(ret == 0)
            {
				sprintf(out, "%s", path);
				free(list);
				free(buffer);
				return 0;
            } 
        }
    }
	
	free(list);
	free(buffer);
	
	return -1;
}

s32 search_and_read_dol(u64 titleid, u8 **contentBuf, u32 *contentSize, bool skip_bootcontent)
{
	char filepath[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	int ret;
	u16 bootindex;
	u16 bootcontent;
	bool bootcontent_loaded;
	
	u8 *tmdBuffer = NULL;
	u32 tmdSize;
	tmd_content *p_cr = NULL;

	// Opening the tmd only to get to know which content is the apploader

	sprintf(filepath, "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
	ret = read_full_file_from_nand(filepath, &tmdBuffer, &tmdSize);
	if (ret < 0)
	{
		return ret;
	}
	
	bootindex = ((tmd *)SIGNATURE_PAYLOAD((signed_blob *)tmdBuffer))->boot_index;
	p_cr = TMD_CONTENTS(((tmd *)SIGNATURE_PAYLOAD((signed_blob *)tmdBuffer)));
	bootcontent = p_cr[bootindex].cid;

	free(tmdBuffer);

	// Write bootcontent to filepath and overwrite it in case another .dol is found
	sprintf(filepath, "/title/%08x/%08x/content/%08x.app", TITLE_UPPER(titleid), TITLE_LOWER(titleid), bootcontent);

	if (skip_bootcontent)
	{
		bootcontent_loaded = false;
			
		// Search the folder for .dols and ignore the apploader
		ret = check_dol(titleid, filepath, bootcontent);
		if (ret < 0)
		{
			return ret;
		}
	} else
	{
		bootcontent_loaded = true;
	}
	
	ret = read_full_file_from_nand(filepath, contentBuf, contentSize);
	if (ret < 0)
	{
		return ret;
	}
	
	if (isLZ77compressed(*contentBuf))
	{
		u8 *decompressed;
		ret = decompressLZ77content(*contentBuf, *contentSize, &decompressed, contentSize, 0);
		if (ret < 0)
		{
			free(*contentBuf);
			return ret;
		}
		free(*contentBuf);
		*contentBuf = decompressed;
	}	
	
	if (bootcontent_loaded)
	{
		return 1;
	} else
	{
		return 0;
	}
}

int tfboot(s_run *run)
	{
	s_nandbooter nb;
	u8 *tfb = (u8 *) 0x90000000;
	
	if (run->nand == NAND_REAL)
		{
		Shutdown (0);
		
		WII_Initialize();
		WII_LaunchTitle((u64)(run->channel.titleId));
		exit(0);  // Use exit() to exit a program, do not use 'return' from main()
		}
	
	// Copy the triiforce image
	memcpy(EXECUTE_ADDR, nandbooter_dol, nandbooter_dol_size);
	DCFlushRange((void *) EXECUTE_ADDR, nandbooter_dol_size);

	// Load the booter
	memcpy(BOOTER_ADDR, booter_dol, booter_dol_size);
	DCFlushRange(BOOTER_ADDR, booter_dol_size);

	entrypoint hbboot_ep = (entrypoint) BOOTER_ADDR;
	
	// Shutdown all system
	Shutdown (0);
	
	// Ok, copy execution data to 0x90000000, nandbooter will read it
	nb.nand = run->nand;
	strcpy (nb.nandPath, run->nandPath);
	memcpy (&nb.channel, &run->channel, sizeof (s_channelConfig));
	memcpy (tfb, &nb, sizeof(s_nandbooter));
	DCFlushRange((void *) tfb, sizeof(s_nandbooter));
	
	// bootit !
	u32 level;

	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(level);
	__exception_closeall();
	hbboot_ep();
	_CPU_ISR_Restore(level);

	return 0;
	}
