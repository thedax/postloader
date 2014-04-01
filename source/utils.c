#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include <sys/stat.h> //for mkdir 
#include "bin2o.h"
#include "globals.h"
#include "devices.h"

extern void __exception_closeall();

u32 get_msec(bool reset)
{
	static u32 startms = 0;
	
	if (reset) startms = ticks_to_millisecs(gettime());

	return ticks_to_millisecs(gettime()) - startms;
}

// NandExits check if on the selected device there is a nand folder (on the root by now)
bool NandExits (int dev)
	{
	char path[PATHMAX];
	
	if (!devices_Get (dev)) return FALSE;
	
	sprintf (path, "%s://title/00000001", devices_Get (dev));
	return fsop_DirExist(path);
	}

void mssleep (int ms)
	{
	usleep (ms * 1000);
	}
	
void convertToUpperCase(char *sPtr)
    {
    while(*sPtr != '\0')
		{
		if (islower((int)*sPtr))
			*sPtr = toupper((int)*sPtr);
			
		sPtr++;
		}
    }
	
// Convert passed buffer to ascii
char* Bin2HexAscii (void* buff, size_t insize, size_t*outsize)
	{
	int i;
	int osize;
	char *ob;
	char hex[8];
	u8 *b = (u8*) buff;
	
	osize = (insize*2)+2;	// Give some extra space... may be needed
	
	ob = calloc (1, osize);
	if (!ob) return NULL;
	
	for (i = 0; i < insize; i++)
		{
		sprintf (hex, "%02X", b[i]);
		strcat (ob, hex);
		}

	ob[osize-1] = '\0';
	if (outsize) *outsize = osize;
	return ob;
	}
	
// Convert passed ascii string to buff. the buffer must contain
int HexAscii2Bin (char *ascii, void* buff)
	{
	int i, j = 0;
	u8 l,h;
	u8* p;
	
	l = strlen(ascii);
	
	p = buff;
	
	for (i = 0; i < strlen(ascii);)
		{
		h = 0;
		l = 0;
		
		if (ascii[i] >= 48 && ascii[i] <= 57) h = ascii[i] - 48;
		if (ascii[i] >= 65 && ascii[i] <= 70) h = ascii[i] - 65 + 10;
		if (ascii[i] >= 97 && ascii[i] <=102) h = ascii[i] - 65 + 10;
		i++;

		if (ascii[i] >= 48 && ascii[i] <= 57) l = ascii[i] - 48;
		if (ascii[i] >= 65 && ascii[i] <= 70) l = ascii[i] - 65 + 10;
		if (ascii[i] >= 97 && ascii[i] <=102) l = ascii[i] - 65 + 10;
		i++;
		
		*p = l + (h * 16);
		
		j++;
		p++;
		}
	
	return j;
	}
	
bool IsPngBuff (u8 *buff, int size)
	{
	if (size < 8 || buff == NULL) return false;
	
	int i;
	u8 sig[8] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};

	for (i = 0; i < 8; i++)
		if (buff[i] != sig[i])
			return false;
	
	return true;
	}
	
void LoadTitlesTxt (void)
	{
	char txtpath[64];
	sprintf (txtpath, "%s://ploader/titles.txt", vars.defMount);

	Video_WaitPanel (TEX_HGL, 0, "Please wait...|Loading titles.txt");
	
	//mt_Lock();
	vars.titlestxt = (char*)fsop_ReadFile (txtpath, 0, NULL);
	//mt_Unlock();
	}
	
	
static void DoMini(u8* kbuf, size_t kernel_size)  
	{  
	kernel_size +=3;
	kernel_size &= 0xFFFFFFFC;		

	u8* mini = (u8*) memalign( 32, armboot_bin_size + kernel_size + 4 );  

	if( !mini )  
		{  
		Debug( "Cannot allocate mini buffer %d, %d", armboot_bin_size, kernel_size);  
		return;  
		}  
	Debug( "mini buffer: %p", mini );  

	memcpy( mini, kbuf, kernel_size);
	DCFlushRange( mini, kernel_size );  
	free(kbuf);
	
	memcpy( mini+kernel_size+4, armboot_bin, armboot_bin_size );  
	DCFlushRange( mini+kernel_size+4, armboot_bin_size );  

	Debug( "armboot.bin copied" );  
	*(u32*)0xc150f000 = 0x424d454d;  
	asm volatile( "eieio" );  

	// physical address for armboot.bin.  ( virtualToPhysical() )  
	*(u32*)0xc150f004 = MEM_VIRTUAL_TO_PHYSICAL( mini+kernel_size+4 );  

	asm volatile( "eieio" );  

	Debug( "physical memory address: %08x", MEM_VIRTUAL_TO_PHYSICAL( mini ) );  
	Debug( "loading bootmii IOS" );  

	// pass position of kernel.bin to mini		
	*(u32*)0x8132FFF0 = MEM_VIRTUAL_TO_PHYSICAL( mini );
	asm volatile( "eieio" );  
	DCFlushRange((void*)0x8132FFF0,4);

	// pass length of kernel.bin to mini        
	*(u32*)0x8132FFF4 = kernel_size;
	asm volatile( "eieio" );  
	DCFlushRange((void*)0x8132FFF4,4);

	IOS_ReloadIOS( 0xfe );  

	Debug( "well shit.  this shouldnt happen" );  

	free( mini );  
	}

static u8 *kernel = NULL;
static size_t kernelsize = 0;
bool Neek2oLoadKernel (void)
	{
	char path[256];
	
	Debug ("Neek2oLoadKernel: begin");

	//mt_Lock();
	
	*path = '\0';
	if (devices_Get(DEV_USB))
		{
		sprintf (path, "%s://sneek/kernel.bin", devices_Get(DEV_USB));
		kernel = fsop_ReadFile (path, 0, &kernelsize);
		}
	if (!kernel && devices_Get(DEV_SD))
		{
		sprintf (path, "%s://sneek/kernel.bin", devices_Get(DEV_SD));
		kernel = fsop_ReadFile (path, 0, &kernelsize);
		}
		
	//mt_Lock();

	Debug ("Neek2oLoadKernel: found on %s (size = %d)", path, kernelsize);
	Debug ("Neek2oLoadKernel: end (0x%X)", kernel);
	
	if (!kernel ) return false;
	return true;
	}

bool Neek2oBoot (void)
	{
	if (!kernel ) return false;
	DoMini (kernel, kernelsize);
	return true;
	}

bool ReloadPostloader (void)
	{
	char path[64];
	int ret;
	
	if (devices_Get(DEV_SD))
		{
		sprintf (path, "%s://apps/postloader/boot.dol", devices_Get(DEV_SD));
		//mt_Lock();
		ret = fsop_FileExist (path);
		//mt_Unlock();
		if (ret) DirectDolBoot (path, NULL, 0);
		}

	if (devices_Get(DEV_USB))
		{
		sprintf (path, "%s://apps/postloader/boot.dol", devices_Get(DEV_USB));
		//mt_Lock();
		ret = fsop_FileExist (path);
		//mt_Unlock();
		}
	
	return false;
	}
	
bool ReloadPostloaderChannel (void)
	{
	WII_Initialize();
	WII_LaunchTitle(TITLE_ID(0x00010001,0x504f5354)); // postLoader3 Channel POST
	return false;
	}
	
/*
This function will just check if there is a readable disc in drive
*/
#define IOCTL_DI_READID	0x70
#define IOCTL_DI_STOPMOTOR 0xE3
#define IOCTL_DI_RESET 0x8A
s32 CheckDisk(void *id)
	{
	const char di_fs[] ATTRIBUTE_ALIGN(32) = "/dev/di";
	s32 di_fd = -1;
	u32 inbuf[8]  ATTRIBUTE_ALIGN(32);
	u32 outbuf[8] ATTRIBUTE_ALIGN(32);	
	
	/* Open "/dev/di" */
	di_fd = IOS_Open(di_fs, 0);
	if (di_fd < 0) return di_fd; // error

	/* Reset drive */
	memset(inbuf, 0, sizeof(inbuf));
	inbuf[0] = IOCTL_DI_RESET << 24;
	inbuf[1] = 1;

	s32 ret = IOS_Ioctl(di_fd, IOCTL_DI_RESET, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));
	if (ret < 0) 
		{
		IOS_Close(di_fd);
		return ret;
		}

	/* Read disc ID */
	memset(inbuf, 0, sizeof(inbuf));
	inbuf[0] = IOCTL_DI_READID << 24;
	ret = IOS_Ioctl(di_fd, IOCTL_DI_READID, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));
	if (ret < 0) return ret;
	if (ret == 1 && id)
		memcpy(id, outbuf, sizeof(dvddiskid));
		
	/* Stop motor */
	memset(inbuf, 0, sizeof(inbuf));
	inbuf[0] = IOCTL_DI_STOPMOTOR << 24;
	IOS_Ioctl(di_fd, IOCTL_DI_STOPMOTOR, inbuf, sizeof(inbuf), outbuf, sizeof(outbuf));
	IOS_Close(di_fd);

	if (ret != 1)
		{
		grlib_menu (0, "Sorry, no disc is detected in your WII", " OK ");
		}
	return ret;
	}
	
void bsort(_PTR __base, size_t __nmemb, size_t __size, int(*_compar)(const _PTR, const _PTR))
	{
	if (__nmemb == 0) return;
	
	int i;
    int mooved;
	void *buff = malloc (__size);
	
// Sort by filter, use stupid algorithm...
	do
		{
		mooved = 0;
		
		for (i = 0; i < __nmemb - 1; i++)
			{
			_PTR el1 = __base+(i * __size);
			_PTR el2 = el1 + __size;
			
			if (_compar (el1, el2))
				{
				// swap
				memcpy (buff, el2, __size);
				memcpy (el2, el1, __size);
				memcpy (el1, buff, __size);

				mooved = 1;
				}
			}
		}
	while (mooved);
	
	free (buff);

	return;
	}
	
bool CheckvWii (void) {
	#ifdef DOLPHIN
	return false;
	#endif
	// Check Device ID
	u32 deviceID = 0;
	ES_GetDeviceID(&deviceID);
	// If it is greater than or equal to 0x20000000 (536870912), then you are running on a Wii U
	return (deviceID >= 536870912);
} // CheckvWii

void DumpFolders (char *folder)
	{
	int i;
	Debug ("DumpRootFolders: begin");
	
	for (i = 0; i < DEV_MAX; i++)
		{
		if (devices_Get(i))
			{
			char path[64];
			
			sprintf (path, "%s:%s", devices_Get(i), folder);
			
			Debug ("DumpRootFolders: %s", path);
			
			DIR *pdir;
			struct dirent *pent;
			
			//mt_Lock();
			pdir=opendir(path);
			
			if (!pdir)
				Debug ("DumpRootFolders: ERROR !");
			
			while ((pent=readdir(pdir)) != NULL) 
				{
				if (pent->d_type == DT_DIR)
					{
					Debug ("> '%s' (dir)", pent->d_name);
					}
				else
					{
					Debug ("> '%s'", pent->d_name);
					}
				}
				
			closedir (pdir);

			//mt_Unlock();
			}
		}
	
	Debug ("DumpRootFolders: end");
	}