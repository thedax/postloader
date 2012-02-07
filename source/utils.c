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

extern void __exception_closeall();

u32 get_msec(bool reset)
{
	static u32 startms = 0;
	
	if (reset) startms = ticks_to_millisecs(gettime());

	return ticks_to_millisecs(gettime()) - startms;
}

/////////////////////////////////////////////////
// Will read a fat file, and return allocated buffer with it's contents.
// err can be (if no information needed, set it to NULL):
//  0: no error
// -1: unable to open the file
// -2: zero file lenght
// -3: cannot allocate buffer
// ret NULL if an error or a pointer to U8 buffer

u8 *ReadFile2Buffer (char *path, size_t *filesize, int *err, bool silent)
	{
	u8 *buff = NULL;
	int size;
	int bytes;
	int block = 65536;
	FILE* f = NULL;
	
	Debug ("begin ReadFile2Buffer %s", path);
	
	if (filesize) *filesize = 0;
	if (err) *err = 0;
	
	f = fopen(path, "rb");
	if (!f)
		{
		if (err != NULL) *err = -1;
		return NULL;
		}

	//Get file size
	fseek( f, 0, SEEK_END);
	size = ftell(f);
	if (filesize) *filesize = size;
	
	if (size <= 0)
		{
		if (err != NULL) *err = -2;
		fclose (f);
		return NULL;
		}
		
	// Return to beginning....
	fseek( f, 0, SEEK_SET);
	
	buff = malloc (size+1);
	if (buff == NULL) 
		{
		if (err != NULL) *err = -3;
		fclose (f);
		return NULL;
		}
	
	bytes = 0;
	do
		{
		bytes += fread(&buff[bytes], 1, block, f );
		if (!silent) MasterInterface (0, 0, 2, "Loading DOL...\n%d %% done", (bytes * 100) / size);
		}
	while (bytes < size);

	fclose (f);
	
	buff[size] = 0;
	
	Debug ("end ReadFile2Buffer, readed %d of %d bytes", bytes, size);

	return buff;
	}
	
// NandExits check if on the selected device there is a nand folder (on the root by now)
bool NandExits (int dev)
	{
	char path[PATHMAX];
	
	if (vars.mount[dev][0] == '\0') return FALSE;
	
	sprintf (path, "%s://title/00000001", vars.mount[dev]);
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
	
void CleanTitleConfiguration(void)
	{
	DIR *pdir;
	struct dirent *pent;
	char path[300], fn[300];
	
	int item = grlib_menu ("Are you sure to clean channels/games configuration ?\n\n(Yes, it is a safe operation)", "Yes##1|No##-1");
	if (item < 0) return;
	
	sprintf (path, "%s://ploader/config", vars.defMount);
	
	pdir=opendir(path);
	
	while ((pent=readdir(pdir)) != NULL) 
		{
		if (strstr (pent->d_name, ".cfg"))
			{
			Video_WaitPanel (TEX_HGL, "Cleaning %s", pent->d_name);

			sprintf (fn, "%s://ploader/config/%s", vars.defMount, pent->d_name);
			unlink (fn);
			}
		}
	
	closedir(pdir);
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
	static bool alreadyDone = false;
	
	if (alreadyDone) return;
	alreadyDone = true;
	
	char txtpath[64];
	sprintf (txtpath, "%s://ploader/titles.txt", vars.defMount);

	Video_WaitPanel (TEX_HGL, "Please wait...|Loading titles.txt");
	
	titlestxt = cfg_Alloc (txtpath, 0);
	if (titlestxt->tag == NULL)
		{
		cfg_Free (titlestxt);
		titlestxt = NULL;
		}
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

	*path = '\0';
	if (IsDevValid(DEV_USB))
		{
		sprintf (path, "%s://sneek/kernel.bin", vars.mount[DEV_USB]);
		kernel = fsop_ReadFile (path, 0, &kernelsize);
		}
	if (!kernel && IsDevValid(DEV_SD))
		{
		sprintf (path, "%s://sneek/kernel.bin", vars.mount[DEV_SD]);
		kernel = fsop_ReadFile (path, 0, &kernelsize);
		}

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
