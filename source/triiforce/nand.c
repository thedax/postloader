#include <stdio.h>
#include <ogcsys.h>
#include <string.h>
#include <malloc.h>

#include "nand.h"
#include "tools.h"

void Debug (const char *text, ...);

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
/*
s32 Nand_Mount(nandDevice *dev)
{
	s32 fd, ret;

	// Open FAT module
	fd = IOS_Open("fat", 0);
	if (fd < 0)
		return fd;

	// TODO Tell the cIOS which partition to use
	
	// Mount device
	ret = IOS_Ioctlv(fd, dev->mountCmd, 0, 0, NULL);

	// Close FAT module
	IOS_Close(fd);

	return ret;
}
*/
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
