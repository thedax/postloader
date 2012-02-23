#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gccore.h>
#include <dirent.h>
#include <wiiuse/wpad.h>
#include <sys/unistd.h>

#include "fsop.h"
#include "debug.h"
#include "../build/bin2o.h"


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
	sprintf (path, "usb://sneek/kernel.bin");
	kernel = fsop_ReadFile (path, 0, &kernelsize);

	if (!kernel)
		{
		sprintf (path, "sd://sneek/kernel.bin");
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
