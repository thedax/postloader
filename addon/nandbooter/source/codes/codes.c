/*
config_bytes[2] hooktypes
0 no
1 VBI
2 KPAD read
3 Joypad Hook
4 GXDraw Hook
5 GXFlush Hook
6 OSSleepThread Hook
7 AXNextFrame Hook
8 Default

config_bytes[4] ocarina
0 no
1 yes

config_bytes[5] paused start
0 no
1 yes


config_bytes[7] debugger
0 no
1 yes
*/

#include <gccore.h>
#include <string.h>
#include <fat.h>
//#include <ogc/usbstorage.h>
#include <usbstorage.h>
#include <sdcard/wiisd_io.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/unistd.h>



#include "codehandler.h"
#include "codehandlerslota.h"
#include "codehandleronly.h"
#include "multidol.h"
#include "patchcode.h"

#include "tools.h"
#include "codes.h"

static const u8 *codelistend = (u8 *) 0x80003000;

static u8 *codebuff = NULL;
static size_t codebuff_size = 0;

void *codelist;

extern u32 ocarinaoption;
extern u32 debuggeroption;
extern u32 hooktypeoption;

DISC_INTERFACE storage;

s32 load_codes(char *filename, u32 maxsize, u8 *buffer)
{
	char text[8];	
	memset(text, 0 , 8);
	
	s32 Fd;
	u32 filesize;
	u32 ret = 0;
	char path[128];
	
	// Read the codes from NAND
	if (ocarinaoption == 1)
		{
		text[0] = 'N';
		text[1] = 'A';
		text[2] = 'N';
		text[3] = 'D';

		sprintf(path, "/TriiForce/codes/%s.gct", filename);
		Fd = ISFS_Open(path, ISFS_OPEN_READ);
		if (Fd < 0)
			{
			sprintf(path, "/codes/%s.gct", filename);
			Fd = ISFS_Open(path, ISFS_OPEN_READ);
			}
		if (Fd < 0)
			{
			debug("Failed to open %s\n", path);
			debug("No %s codes found\n", text);
			sleep(3);
			return -1;
			}

		fstats *status = allocate_memory(sizeof(fstats));
		if (status == NULL)
			{
			debug("Out of memory for status\n");
			ISFS_Close(Fd);
			return -1;
			}
		
		ret = ISFS_GetFileStats(Fd, status);
		if (ret < 0)
			{
			debug("ISFS_GetFileStats failed %d\n", ret);
			ISFS_Close(Fd);
			free(status);
			return -1;
			}
		filesize = status->file_length;
		free(status);
		
		if (filesize > maxsize)
			{
			debug("Too many codes\n");
			ISFS_Close(Fd);
			return -1;
			}
			
		// The codes offset is never aligned, but nand reads need to be
		u8 *buf = allocate_memory(filesize);
		if (buf == NULL)
			{
			debug("Out of memory for buffer\n");
			ISFS_Close(Fd);
			return -1;
			}
			
		ret = ISFS_Read(Fd, buf, filesize);
		if (ret < 0)
			{
			debug("ISFS_Read failed %d\n", ret);
			ISFS_Close(Fd);
			free(buf);
			return ret;
			}
		ISFS_Close(Fd);
		
		memcpy(buffer, buf, filesize);
		free(buf);		
		} 
	else
		// Get codes from in memory buffer
		{
		if (codebuff && codebuff_size)
			{
			debug("Codes found in memory !\n");
			memcpy (buffer, codebuff, codebuff_size);
			sleep (3);
			}
		else
			{
			debug("Codes not found in memory\n");
			return -1;
			sleep (3);
			}
		}

	return 0;
	}		



//---------------------------------------------------------------------------------
void load_handler()
//---------------------------------------------------------------------------------
{
	if (hooktypeoption != 0x00)
	{
		if (debuggeroption > 0)
		{
			switch(gecko_channel)
			{
				case 0: // Slot A
					memset((void*)0x80001800,0,codehandlerslota_size);
					memcpy((void*)0x80001800,codehandlerslota,codehandlerslota_size);
					//if (debuggeroption == 0x02)
					//	*(u32*)0x80002798 = 1;
					memcpy((void*)0x80001CDE, &codelist, 2);
					memcpy((void*)0x80001CE2, ((u8*) &codelist) + 2, 2);
					memcpy((void*)0x80001F5A, &codelist, 2);
					memcpy((void*)0x80001F5E, ((u8*) &codelist) + 2, 2);
					DCFlushRange((void*)0x80001800,codehandlerslota_size);
					break;
					
				case 1:	// slot B
					memset((void*)0x80001800,0,codehandler_size);
					memcpy((void*)0x80001800,codehandler,codehandler_size);
					if (debuggeroption == 0x02)
						*(u32*)0x80002774 = 1;
					memcpy((void*)0x80001CDE, &codelist, 2);
					memcpy((void*)0x80001CE2, ((u8*) &codelist) + 2, 2);
					memcpy((void*)0x80001F5A, &codelist, 2);
					memcpy((void*)0x80001F5E, ((u8*) &codelist) + 2, 2);
					DCFlushRange((void*)0x80001800,codehandler_size);
					break;
					
				case 2:
					memset((void*)0x80001800,0,codehandler_size);
					memcpy((void*)0x80001800,codehandler,codehandler_size);
					if (debuggeroption == 0x02)
						*(u32*)0x80002774 = 1;
					memcpy((void*)0x80001CDE, &codelist, 2);
					memcpy((void*)0x80001CE2, ((u8*) &codelist) + 2, 2);
					memcpy((void*)0x80001F5A, &codelist, 2);
					memcpy((void*)0x80001F5E, ((u8*) &codelist) + 2, 2);
					DCFlushRange((void*)0x80001800,codehandler_size);
					break;
			}
		}
		else
		{
			memset((void*)0x80001800,0,codehandleronly_size);
			memcpy((void*)0x80001800,codehandleronly,codehandleronly_size);
			memcpy((void*)0x80001906, &codelist, 2);
			memcpy((void*)0x8000190A, ((u8*) &codelist) + 2, 2);
			DCFlushRange((void*)0x80001800,codehandleronly_size);
		}
		// Load multidol handler
		memset((void*)0x80001000,0,multidol_size);
		memcpy((void*)0x80001000,multidol,multidol_size); 
		DCFlushRange((void*)0x80001000,multidol_size);
		switch(hooktypeoption)
		{
			case 0x01:
				memcpy((void*)0x8000119C,viwiihooks,12);
				memcpy((void*)0x80001198,viwiihooks+3,4);
				break;
			case 0x02:
				memcpy((void*)0x8000119C,kpadhooks,12);
				memcpy((void*)0x80001198,kpadhooks+3,4);
				break;
			case 0x03:
				memcpy((void*)0x8000119C,joypadhooks,12);
				memcpy((void*)0x80001198,joypadhooks+3,4);
				break;
			case 0x04:
				memcpy((void*)0x8000119C,gxdrawhooks,12);
				memcpy((void*)0x80001198,gxdrawhooks+3,4);
				break;
			case 0x05:
				memcpy((void*)0x8000119C,gxflushhooks,12);
				memcpy((void*)0x80001198,gxflushhooks+3,4);
				break;
			case 0x06:
				memcpy((void*)0x8000119C,ossleepthreadhooks,12);
				memcpy((void*)0x80001198,ossleepthreadhooks+3,4);
				break;
			case 0x07:
				memcpy((void*)0x8000119C,axnextframehooks,12);
				memcpy((void*)0x80001198,axnextframehooks+3,4);
				break;
			case 0x08:
				//if (customhooksize == 16)
				//{
				//	memcpy((void*)0x8000119C,customhook,12);
				//	memcpy((void*)0x80001198,customhook+3,4);
				//}
				break;
			case 0x09:
				//memcpy((void*)0x8000119C,wpadbuttonsdownhooks,12);
				//memcpy((void*)0x80001198,wpadbuttonsdownhooks+3,4);
				break;
			case 0x0A:
				//memcpy((void*)0x8000119C,wpadbuttonsdown2hooks,12);
				//memcpy((void*)0x80001198,wpadbuttonsdown2hooks+3,4);
				break;
		}
		DCFlushRange((void*)0x80001198,16);
	}
}


void do_codes(u64 titleid)
	{
	s32 ret = 0;
	char gameidbuffer[8];
	
	debug("do_codes...");
	
	memset(gameidbuffer, 0, 8);
	
	gameidbuffer[0] = (titleid & 0xff000000) >> 24;
	gameidbuffer[1] = (titleid & 0x00ff0000) >> 16;
	gameidbuffer[2] = (titleid & 0x0000ff00) >> 8;
	gameidbuffer[3] = (titleid & 0x000000ff);

	if (debuggeroption == 0x00)
		codelist = (u8 *) 0x800022A8;
	else
		codelist = (u8 *) 0x800028B8;

	load_handler();
	memcpy((void *)0x80001800, gameidbuffer, 6);

	if (ocarinaoption != 0)
		{
		memset(codelist, 0, (u32)codelistend - (u32)codelist);
		
		ret = load_codes(gameidbuffer, (u32)codelistend - (u32)codelist, codelist);
		if (ret >= 0)
			{
			debug("Codes found. Applying\n");
			}
		else
			{
			debug("Codes not found.\n");
			}
		//sleep(1);
		}

	DCFlushRange((void*)0x80000000, 0x3f00);
	ICInvalidateRange((void*)0x80000000, 0x3f00);
	
	debug("done\n");
	}

void preload_codes (u64 titleid)
	{
	size_t maxsize;
	char filename[8];
	
	if (!ocarinaoption) return;
	
	debug("Preloading codes\n");
	//sleep (1);
	
	if (debuggeroption == 0x00)
		codelist = (u8 *) 0x800022A8;
	else
		codelist = (u8 *) 0x800028B8;
	
	maxsize = (u32)codelistend - (u32)codelist;
	
	memset(filename, 0, 8);
	
	filename[0] = (titleid & 0xff000000) >> 24;
	filename[1] = (titleid & 0x00ff0000) >> 16;
	filename[2] = (titleid & 0x0000ff00) >> 8;
	filename[3] = (titleid & 0x000000ff);

	char text[8];	
	memset(text, 0 , 8);
	
	FILE *fp;
	u32 ret;
	char path[128];
	
	// Read the codes from NAND
	switch (ocarinaoption)
		{
		case 2:
			text[0] = 'S';
			text[1] = 'D';
			storage = __io_wiisd;
		break;
		
		case 3:
			text[0] = 'U';
			text[1] = 'S';
			text[2] = 'B';
			//storage = __io_usbstorage;
			storage = __io_wiiums;
		break;
		
		default:
			codebuff = NULL;
			codebuff_size = 0;
			
			return;
		break;
		}

	ret = storage.startup();
	if (ret < 0) 
		{
		debug("%s Error\n", text);
		sleep (3);
		return;
		}
	
	ret = fatMountSimple("fat", &storage);

	if (ret < 0) 
		{
		storage.shutdown();
		debug("FAT Error\n");
		sleep (3);
		return;
		}

	sprintf(path, "fat:/codes/%s.gct", filename);
	fp = fopen(path, "rb");

	if (!fp) 
		{
		fatUnmount("fat");
		storage.shutdown();
		debug("Failed to open %s\n", path);
		debug("No %s codes found\n", text);
		sleep(3);
		return;
		}

	fseek(fp, 0, SEEK_END);
	codebuff_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (codebuff_size > maxsize || codebuff_size == 0)
		{
		fclose(fp);
		fatUnmount("fat");
		storage.shutdown();
		if (codebuff_size)
			debug("Too many codes\n");
		else
			debug("File is empty\n");
			
		sleep (3);
		return;
		}	
	
	ret = 0;

	codebuff = malloc (codebuff_size);
	if (codebuff)
		ret = fread(codebuff, 1, codebuff_size, fp);

	fclose(fp);
	fatUnmount("fat");
	storage.shutdown();

	if (ret != codebuff_size)
	{	
		debug("%s Code Error\n", text);
		sleep (3);
		
		free (codebuff);
		
		codebuff = NULL;
		codebuff_size = 0;
		return;
	}

	debug("Code loaded (size = %d)\n", codebuff_size);
	//sleep (1);
	
	return;
	}