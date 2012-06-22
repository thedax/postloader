 /*******************************************************************************
 * main.c
 *
 * Copyright (c) 2009 The Lemon Man
 * Copyright (c) 2009 Nicksasa
 * Copyright (c) 2009 WiiPower
 * Copyright (c) 2011 stfour
 *
 * Distributed under the terms of the GNU General Public License (v2)
 * See http://www.gnu.org/licenses/gpl-2.0.txt for more info.
 *
 * Description:
 * -----------
 *
 ******************************************************************************/
 
//#define DEBUGMODE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <stdlib.h>
#include <gccore.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>
#include <network.h>

#include "main.h"
#include "tools.h"
#include "isfs.h"
#include "lz77.h"
#include "patch.h"
#include "codes/codes.h"
#include "codes/patchcode.h"
#include "nand.h"

#define VER "2.4"

// This must reflect postloader ---------------------------------------------------------------
typedef struct
	{
	int priority;	// Vote !
	bool hidden;	// if 1, this app will be not listed
	
	u64 titleId; 	// title id
	u8 ios;		 	// ios to reload
	u8 vmode;	 	// 0 Default Video Mode	1 NTSC480i	2 NTSC480p	3 PAL480i	4 PAL480p	5 PAL576i	6 MPAL480i	7 MPAL480p
	s8 language; 	//	-1 Default Language	0 Japanese	1 English	2 German	3 French	4 Spanish	5 Italian	6 Dutch	7 S. Chinese	8 T. Chinese	9 Korean
	u8 vpatch;	 	// 0 No Video patches	1 Smart Video patching	2 More Video patching	3 Full Video patching
	u8 hook;	 	// 0 No Ocarina&debugger	1 Hooktype: VBI	2 Hooktype: KPAD	3 Hooktype: Joypad	4 Hooktype: GXDraw	5 Hooktype: GXFlush	6 Hooktype: OSSleepThread	7 Hooktype: AXNextFrame
	u8 ocarina; 	// 0 No Ocarina	1 Ocarina from NAND 	2 Ocarina from SD	3 Ocarina from USB"
	u8 bootMode;	// 0 Normal boot method	1 Load apploader
	}
s_channelConfig;

typedef struct
	{
	u8 nand; 	 	// 0 no nand emu	1 sd nand emu	2 usb nand emu
	char nandPath[64];			// Folder of the nand
	s_channelConfig channel;
	}
s_nandbooter;

// --------------------------------------------------------------------------------------------


extern s32 __IOS_ShutdownSubsystems();
extern void __exception_closeall();

u32 *xfb = NULL;
GXRModeObj *rmode = NULL;
u8 Video_Mode;

void*	dolchunkoffset[64];			//TODO: variable size
u32		dolchunksize[64];			//TODO: variable size
u32		dolchunkcount;

void _unstub_start();

s32 videooption;
u32 languageoption;
u32 videopatchoption;

u32 hooktypeoption;
u32 debuggeroption;
u32 ocarinaoption;

u32 bootmethodoption;

void StoreLogFile (void);

void green_fix(void); //GREENSCREEN FIX

// Prevent IOS36 loading at startup
s32 __IOS_LoadStartupIOS()
{
	return 0;
}

void reboot()
{
	Disable_Emu();

	debug ("Trying to write error log to sd...\n");
	StoreLogFile ();
	
	/*
	if (strncmp("STUBHAXX", (char *)0x80001804, 8) == 0)
	{
		debug("Exiting to HBC...\n");
		sleep(3);
		exit(0);
	}
	*/
	debug("Rebooting...\n");
	SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

typedef void (*entrypoint) (void);

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

s32 IOSReloadBlock(u8 reqios, bool enable)
{
	s32 ESHandle = IOS_Open("/dev/es", 0);

	if (ESHandle < 0)
	{
		debug("Reload IOS Block failed, cannot open /dev/es\n");
		return ESHandle;
	}

	static ioctlv vector[2] ATTRIBUTE_ALIGN(32);
	static u32 mode ATTRIBUTE_ALIGN(32);
	static u32 ios ATTRIBUTE_ALIGN(32);

	mode = enable ? 2 : 0;
	vector[0].data = &mode;
	vector[0].len  = sizeof(u32);

	if (enable) {
		ios = reqios;
		vector[1].data = &ios;
		vector[1].len  = sizeof(u32);
	}

	s32 r = IOS_Ioctlv(ESHandle, 0xA0, 2, 0, vector);
	debug("%s Block IOS Reload for cIOS%uv%u %s\n", (enable ? "Enable" : "Disable"), IOS_GetVersion(), IOS_GetRevision() % 100, r < 0 ? "FAILED!" : "SUCCEEDED!");

	IOS_Close(ESHandle);

	return r;
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
		debug("ERR: (checkdol) Out of memory\n");
		return -1;
	}
 
    u8 check[6] = {0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
 
    sprintf(path, "/title/%08x/%08x/content", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
    ret = getdir(path, &list, &num);
    if (ret < 0)
	{
		debug("ERR: (checkdol) Reading folder of the title failed\n");
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
				//debug("Found LZ77 compressed content --> %s\n", list[cnt].name);
				//debug("This is most likely the main DOL, decompressing for checking\n");

				// We only need 6 bytes...
				ret = decompressLZ77content(buffer, 32, &decompressed, &decomp_size, 32);
				if (ret < 0)
				{
					debug("ERR: (checkdol) Decompressing failed\n");
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
				debug("Found DOL --> %s\n", list[cnt].name);
				sprintf(out, "%s", path);
				free(list);
				free(buffer);
				return 0;
            } 
        }
    }
	
	free(list);
	free(buffer);
	
	debug("ERR: (checkdol) No .dol found\n");
	return -1;
}

void patch_dol(bool bootcontent)
{
	s32 ret;
	int i;
	bool hookpatched = false;
	
	for (i=0;i < dolchunkcount;i++)
	{		
		if (!bootcontent)
		{
			if (languageoption != -1)
			{
				ret = patch_language(dolchunkoffset[i], dolchunksize[i], languageoption);
				
				// just to make the compiler happy
				if (ret);
			}
			
			if (videopatchoption != 0)
			{
				search_video_modes(dolchunkoffset[i], dolchunksize[i]);
				patch_video_modes_to(rmode, videopatchoption);
			}
		}

		if (hooktypeoption != 0)
		{
			// Before this can be done, the codehandler needs to be in memory, and the code to patch needs to be in the right pace
			if (dochannelhooks(dolchunkoffset[i], dolchunksize[i], bootcontent))
			{
				hookpatched = true;
			}			
		}
	}
	if (hooktypeoption != 0 && !hookpatched)
	{
		debug("ERR: (patch_dol) Could not patch the hook: Ocarina and debugger won't work\n");
	}
}  

#define MEM1_MAX 0x817FFFFF
u32 load_dol(const void *dolstart)
{
    u32 i;
	int changeEP = -1;
    dolheader *dolfile;

    if (dolstart)
    {
        dolfile = (dolheader *) dolstart;
		
		debug("EP: %08x\n", dolfile->entry_point);
		
		if (dolfile->bss_start < MEM1_MAX) 
			{
			u32 size = dolfile->bss_size;
			if (dolfile->bss_start + dolfile->bss_size > MEM1_MAX) 
				size = MEM1_MAX - dolfile->bss_start;
			
			memset((void *)dolfile->bss_start, 0, size);
			DCFlushRange((void *) dolfile->bss_start, dolfile->bss_size);
			ICInvalidateRange((void *)dolfile->bss_start, dolfile->bss_size);
			}

        for (i = 0; i < 7; i++)
        {
			dolchunkoffset[dolchunkcount] = (void *)dolfile->text_start[i];
			dolchunksize[dolchunkcount] = dolfile->text_size[i];
			dolchunkcount++;
		
			if (dolfile->text_start[i] < 0x80000000)
				{
				dolfile->text_start[i] += 0x80000000;
				if (i == 0)
					{
					dolfile->entry_point = dolfile->text_start[i];
					changeEP = i+1;
					}
				}
			
            if ((dolfile->text_size[i] == 0) || (dolfile->text_start[i] < 0x80000000))
				{
				//debug ("skip (%d,%d)\n", (dolfile->text_size[i] == 0), (dolfile->text_start[i] < 0x80000000));
				if (i == 0) changeEP = true;
                continue;
				}

			debug("TS: %u from %08x to %08x-%08x...", i, dolfile->text_pos[i], dolfile->text_start[i], dolfile->text_start[i]+dolfile->text_size[i]);

			if (changeEP == i)
				{
				dolfile->entry_point = dolfile->text_start[i];
				changeEP = false;
				debug ("ok (EP Changed)\n");
				}
			else
				debug ("ok\n");

            memmove((void *) dolfile->text_start[i], dolstart + dolfile->text_pos[i], dolfile->text_size[i]);
            DCFlushRange ((void *) dolfile->text_start[i], dolfile->text_size[i]);
            ICInvalidateRange((void *) dolfile->text_start[i], dolfile->text_size[i]);
        }

        for (i = 0; i < 11; i++)
        {
            if ((dolfile->data_size[i] == 0) || (dolfile->data_start[i] <= 0x100))
				{
                continue;
				}
				
			debug("DS: %u from %08x to %08x-%08x...", i, dolfile->data_pos[i], dolfile->data_start[i], dolfile->data_start[i]+dolfile->text_size[i]);
			debug ("ok\n");

            // VIDEO_WaitVSync();
            memmove((void *) dolfile->data_start[i], dolstart + dolfile->data_pos[i], dolfile->data_size[i]);
            DCFlushRange((void *) dolfile->data_start[i],
                    dolfile->data_size[i]);
        }

        return dolfile->entry_point;
    }
    return 0;
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
	debug("Reading TMD...");

	sprintf(filepath, "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(titleid), TITLE_LOWER(titleid));
	ret = read_full_file_from_nand(filepath, &tmdBuffer, &tmdSize);
	if (ret < 0)
		{
		debug("ERR: (search_and_read_dol) Reading TMD failed\n");
		return ret;
		}
	debug("done\n");
	
	bootindex = ((tmd *)SIGNATURE_PAYLOAD((signed_blob *)tmdBuffer))->boot_index;
	p_cr = TMD_CONTENTS(((tmd *)SIGNATURE_PAYLOAD((signed_blob *)tmdBuffer)));
	bootcontent = p_cr[bootindex].cid;

	free(tmdBuffer);

	// Write bootcontent to filepath and overwrite it in case another .dol is found
	sprintf(filepath, "/title/%08x/%08x/content/%08x.app", TITLE_UPPER(titleid), TITLE_LOWER(titleid), bootcontent);

	if (skip_bootcontent)
		{
		bootcontent_loaded = false;
		debug("Searching for main DOL...\n");
			
		// Search the folder for .dols and ignore the apploader
		ret = check_dol(titleid, filepath, bootcontent);
		if (ret < 0)
			{
			debug("ERR: (search_and_read_dol) Searching for main.dol failed, loading nand loader instead\n");
			bootcontent_loaded = true;
			}
		} 
	else
		{
		bootcontent_loaded = true;
		}
	
    debug("Loading DOL: %s\n", filepath);

	ret = read_full_file_from_nand(filepath, contentBuf, contentSize);
	if (ret < 0)
		{
		debug("ERR: (search_and_read_dol) Reading .dol failed\n");
		return ret;
		}
	
	if (isLZ77compressed(*contentBuf))
		{
		debug("Decompressing...");
		u8 *decompressed;
		ret = decompressLZ77content(*contentBuf, *contentSize, &decompressed, contentSize, 0);
		if (ret < 0)
			{
			debug("ERR: (search_and_read_dol) Decompression failed\n");
			free(*contentBuf);
			return ret;
			}
		free(*contentBuf);
		*contentBuf = decompressed;
		debug("done\n");
		}	

	if (bootcontent_loaded)
		{
		return 1;
		} 
	else
		{
		return 0;
		}
	}


void determineVideoMode(u64 titleid)
{
	if (videooption == 0)
	{
		// Get rmode and Video_Mode for system settings first
		u32 tvmode = CONF_GetVideo();

		// Attention: This returns &TVNtsc480Prog for all progressive video modes
		rmode = VIDEO_GetPreferredMode(0);
		
		switch (tvmode) 
		{
			case CONF_VIDEO_PAL:
				if (CONF_GetEuRGB60() > 0) 
				{
					Video_Mode = VI_EURGB60;
				}
				else 
				{
					Video_Mode = VI_PAL;
				}
				break;

			case CONF_VIDEO_MPAL:
				Video_Mode = VI_MPAL;
				break;

			case CONF_VIDEO_NTSC:
			default:
				Video_Mode = VI_NTSC;
				
		}

		// Overwrite rmode and Video_Mode when Default Video Mode is selected and Wii region doesn't match the channel region
		u32 low;
		low = TITLE_LOWER(titleid);
		char Region = low % 256;
		if (*(char *)&low != 'W') // Don't overwrite video mode for WiiWare
		{
			switch (Region) 
			{
				case 'P':
				case 'D':
				case 'F':
				case 'X':
				case 'Y':
					if (CONF_GetVideo() != CONF_VIDEO_PAL)
					{
						Video_Mode = VI_EURGB60;

						if (CONF_GetProgressiveScan() > 0 && VIDEO_HaveComponentCable())
						{
							rmode = &TVNtsc480Prog; // This seems to be correct!
						}
						else
						{
							rmode = &TVEurgb60Hz480IntDf;
						}				
					}
					break;

				case 'E':
				case 'J':
				case 'T':
					if (CONF_GetVideo() != CONF_VIDEO_NTSC)
					{
						Video_Mode = VI_NTSC;
						if (CONF_GetProgressiveScan() > 0 && VIDEO_HaveComponentCable())
						{
							rmode = &TVNtsc480Prog;
						}
						else
						{
							rmode = &TVNtsc480IntDf;
						}				
					}
			}
		}
	} else
	{
		if (videooption == 1)
		{
			rmode = &TVNtsc480IntDf;
		} else
		if (videooption == 2)
		{
			rmode = &TVNtsc480Prog;
		} else
		if (videooption == 3)
		{
			rmode = &TVEurgb60Hz480IntDf;
		} else
		if (videooption == 4)
		{
			rmode = &TVEurgb60Hz480Prog;
		} else
		if (videooption == 5)
		{
			rmode = &TVPal528IntDf;
		} else
		if (videooption == 6)
		{
			rmode = &TVMpal480IntDf;
		} else
		if (videooption == 7)
		{
			rmode = &TVMpal480Prog;
		}
		Video_Mode = (rmode->viTVMode) >> 2;
	}
}

void setVideoMode()
	{	
	VIDEO_Init();
	VIDEO_SetBlack(TRUE);
	rmode = VIDEO_GetPreferredMode(0);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	
	// Overwrite all progressive video modes as they are broken in libogc
	if (videomode_interlaced(rmode) == 0)
		{
		rmode = &TVNtsc480Prog;
		}

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
	VIDEO_SetBlack(FALSE);
	}

#define	Sys_Magic	((vu32*)0x80000020)
#define	Version		((vu32*)0x80000024)
#define	Arena_L		((vu32*)0x80000030)
#define	Arena_H		((vu32*)0x80000034)
#define	BI2			((vu32*)0x800000F4)
#define	Bus_Speed	((vu32*)0x800000F8)
#define	CPU_Speed	((vu32*)0x800000Fc)

void bootTitle(u64 titleid)
	{
	entrypoint appJump;
	int ret;
	u32 requested_ios;
	u8 *dolbuffer;
	u32 dolsize;
	bool bootcontentloaded;
	
	debug("Running search_and_read_dol\n");

	ret = search_and_read_dol(titleid, &dolbuffer, &dolsize, (bootmethodoption == 0));
	if (ret < 0)
	{
		debug("ERR: (bootTitle) .dol loading failed\n");
		return;
	}
	bootcontentloaded = (ret == 1);

	determineVideoMode(titleid);
	
	//setVideoMode();
	
	entryPoint = load_dol(dolbuffer);

	free(dolbuffer);

	debug(".dol loaded\n");

	ret = identify(titleid, &requested_ios);
	if (ret < 0)
		{
		debug("ERR: (bootTitle) Identify failed\n");
		return;
		}
	
	// Set the clock
	settime(secs_to_ticks(time(NULL) - 946684800));

	/* Setup low memory */
	*(vu32 *)0x80000060 = 0x38A00040;
	*(vu32 *)0x800000E4 = 0x80431A80;
	*(vu32 *)0x800000EC = 0x81800000; // Dev Debugger Monitor Address
	*(vu32 *)0x800000F0 = 0x01800000; // Simulated Memory Size
	*(vu32 *)0xCD00643C = 0x00000000; // 32Mhz on Bus

	// Patch in info missing from apploader reads
	*Sys_Magic      = 0x0d15ea5e;
	*Version        = 1;
	*Arena_L        = 0x00000000;
	*Arena_H        = 0x00000000;
	*BI2            = 0x817E5480;
	*Bus_Speed      = 0x0E7BE2C0;
	*CPU_Speed      = 0x2B73A840;

	// From NeoGamme R4 (WiiPower)
	*(vu32 *)0x800030F0 = 0x0000001C;
	*(vu32 *)0x8000318C = 0x00000000;
	*(vu32 *)0x80003190 = 0x00000000;

    // Game ID Online Check
    memset((void *)0x80000000, 0, 6);
    *(vu32 *)0x80000000 = TITLE_LOWER(titleid);

	// IOS Version Check (002 error)
    *(vu32*)0x80003140 = ((requested_ios << 16)) | 0xFFFF;
    *(vu32*)0x80003188 = ((requested_ios << 16)) | 0xFFFF;

	*(u32 *)0x800000CC = Video_Mode;
	
	DCFlushRange((void*)0x80000000, 0x3F00);
	
	// Memory setup when booting the main.dol
	if (entryPoint != 0x3400)
		{
		/*
		memset((void *)0x817FE000, 0, 0x2000); // Clearing BI2, or should this be read from somewhere?
		DCFlushRange((void*)0x817FE000, 0x2000);		
		*/
		if (hooktypeoption == 0)
			{
			*(u32 *)0x80003180 = TITLE_LOWER(titleid);
			DCFlushRange((void*)0x80003180, 4);
			} 
		else
			{
			*(u32 *)0x80003180 = 0;		// No comment required here
			DCFlushRange((void*)0x80003180, 4);
			}

		*(u32 *)0x80003184 = 0x81000000;	// Game id address, while there's all 0s at 0x81000000 when using the apploader...
		DCFlushRange((void*)0x80003184, 4);
		}
	
	// Maybe not required at all?	
	ret = ES_SetUID(titleid);
	if (ret < 0)
		{
		debug("ERR: (bootTitle) ES_SetUID failed %d", ret);
		return;
		}	

	if (hooktypeoption)
		{
		do_codes(titleid);
		}
	
	patch_dol(bootcontentloaded);
	
	debug("Loading complete, booting...\n");
	
	green_fix ();

	appJump = (entrypoint)entryPoint;

	tell_cIOS_to_return_to_channel();
	
	/*
	IRQ_Disable();
	__IOS_ShutdownSubsystems();
	__exception_closeall();
	*/

	u32 level;
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable(level);
	__exception_closeall();
	
	
	if (entryPoint != 0x3400)
		{
		if (hooktypeoption != 0)
			{
			__asm__(
						"lis %r3, entryPoint@h\n"
						"ori %r3, %r3, entryPoint@l\n"
						"lwz %r3, 0(%r3)\n"
						"mtlr %r3\n"
						"lis %r3, 0x8000\n"
						"ori %r3, %r3, 0x18A8\n"
						"mtctr %r3\n"
						"bctr\n"
						);
						
			} 
		else
			{
			appJump();	
			}
		} 
	else
		{
		if (hooktypeoption != 0)
			{
			__asm__(
						"lis %r3, returnpoint@h\n"
						"ori %r3, %r3, returnpoint@l\n"
						"mtlr %r3\n"
						"lis %r3, 0x8000\n"
						"ori %r3, %r3, 0x18A8\n"
						"mtctr %r3\n"
						"bctr\n"
						"returnpoint:\n"
						"bl DCDisable\n"
						"bl ICDisable\n"
						"li %r3, 0\n"
						"mtsrr1 %r3\n"
						"lis %r4, entryPoint@h\n"
						"ori %r4,%r4,entryPoint@l\n"
						"lwz %r4, 0(%r4)\n"
						"mtsrr0 %r4\n"
						"rfi\n"
						);
			} 
		else
			{
			_unstub_start();
			}
		}
	}

extern char * logbuff;
void StoreLogFile (void)
	{
	fatInitDefault(); // should only be called once

	FILE* file = fopen("sd:/nb.log","wb");

	if(file > 0)
		{
		fwrite(logbuff, 1, strlen(logbuff), file);
		fclose(file);
		}
	}
	
/*
static void Net(void)
	{
	s32 result;
	static char ip[16];
	
	debug ("net_init = %d\n", net_init ());

	result = if_config (ip, NULL, NULL, true);
	debug ("if_config = %d, ip = %s\n", result, ip);

	debug ("net_deinit\n");
	net_deinit ();

	debug ("net_wc24cleanup\n");
	net_wc24cleanup();
	
	return;
	}
*/

bool SenseSneek (bool isfsinit)
	{
	if (isfsinit) ISFS_Initialize ();
	
	u32 num = 0;
	char path[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);

	strcpy (path, "/SNEEK");
	s32 ret = ISFS_ReadDir(path, NULL, &num);
	
	if (isfsinit) ISFS_Deinitialize ();
	
	return (ret == 0);
	}

void InitVideo (void)
	{
	VIDEO_Init();
	VIDEO_SetBlack(TRUE); 
	
	rmode = VIDEO_GetPreferredMode(NULL);

	//apparently the video likes to be bigger then it actually is on NTSC/PAL60/480p. lets fix that!
	if( rmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
	{
		//the correct one would be * 0.035 to be sure to get on the Action safe of the screen.
		GX_AdjustForOverscan(rmode, rmode, 0, rmode->viWidth * 0.026 ); 
	}
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_ClearFrameBuffer (rmode, xfb, 0); 
	
	console_init( xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth*VI_DISPLAY_PIX_SZ );

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	
	// debug("resolution is %dx%d\n",rmode->viWidth,rmode->viHeight);
	}

void green_fix(void) //GREENSCREEN FIX
	{
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(TRUE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
	}

int main(int argc, char* argv[])
	{
	int ret;
	s_nandbooter nb;

	u8 *tfb = ((u8 *) 0x93200000);

	memcpy (&nb, tfb, sizeof(s_nandbooter));
	
	InitVideo ();

#ifdef DEBUGMODE
	debug2Console (true);
#endif	

	Console_SetPosition (12,0);
	PrintCenter ("nandBooter "VER" is loading...", 80);
	
	debug ("NandBooter "VER"\n");
	debug ("\n");
	
	if (ES_GetTitleID(&old_title_id) < 0)
		{
		old_title_id = (0x00010001ULL << 32) | *(u32 *)0x80000000;
		}
	
	int ios[7] = { 249, 250, 251, 252, 247 , 248};
	if (!SenseSneek (true))
		{
		/*
		if (IOS_GetVersion() != ios[nb.channel.ios])
			{
			debug ("IOS_ReloadIOS\n");
			IOS_ReloadIOS(ios[nb.channel.ios]);
			}
		*/
		IOS_ReloadIOS(ios[nb.channel.ios]);
		//Net ();
		//IOSReloadBlock(IOS_GetVersion(), true);
		}
		
	// Configure triiforce options
	videooption = nb.channel.vmode;
	languageoption = nb.channel.language;
	videopatchoption = nb.channel.vpatch;
	hooktypeoption = nb.channel.hook;
	debuggeroption = 0;
	ocarinaoption = nb.channel.ocarina;
	bootmethodoption = nb.channel.bootMode;

	debug("nandBooter (b4): postLoader triiforce mod...\n\n");
	debug("CONF: %d, %d, %d, %d, %d, %d, %d\n", ios[nb.channel.ios], videooption, languageoption, videopatchoption, hooktypeoption, ocarinaoption, bootmethodoption);
	debug("NAND: %d, %s\n\n", nb.nand, nb.nandPath);
	
	debug ("videooption = %d\n", videooption);
	debug ("languageoption = %d\n", languageoption);
	debug ("videopatchoption = %d\n", videopatchoption);
	debug ("hooktypeoption = %d\n", hooktypeoption);
	debug ("ocarinaoption = %d\n", ocarinaoption);
	debug ("bootmethodoption = %d\n", bootmethodoption);
	debug ("\n");
	
	// Preload codes
	if (hooktypeoption) preload_codes ((u64)(nb.channel.titleId));

	if (nb.nand != 0)
		{
		debug("Starting nand emu...\n");
		ret = Enable_Emu(nb.nand, nb.nandPath);
		if (ret < 0)
			{
			debug("ERR: (main) Starting nand emu failed\n");
			reboot();
			}
		}

	ret = ISFS_Initialize();
	debug ("ISFS_Initialize: %d\n", ret);
	
	bootTitle((u64)(nb.channel.titleId));

	sleep(5);
	
	debug ("Things are gone wrong, disabling emulator...\n");
	Disable_Emu ();
	
	// Try to write log to sd
	reboot();
	return 0;
	}
