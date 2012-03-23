#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <ntfs.h>
#include <dirent.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_watchdog.h>
#include <sys/unistd.h>
#include <string.h>
#include <gccore.h>
#include "usbstorage.h"
#include "devices.h"
#include "debug.h"
#include "fsop/fsop.h"

//these are the only stable and speed is good
#define CACHE 8
#define SECTORS 64

const DISC_INTERFACE* storage = NULL;

#define le32(i) (((((u32) i) & 0xFF) << 24) | ((((u32) i) & 0xFF00) << 8) | \
                ((((u32) i) & 0xFF0000) >> 8) | ((((u32) i) & 0xFF000000) >> 24))

static int partinfo[DEV_MAX] = {0}; // Part num, or part num + 10 for ntfs
static int mounted[DEV_MAX] = {0}; // Part num, or part num + 10 for ntfs

static char DeviceName[DEV_MAX][6] =
{
    "sd",
    "usb",
    "usb2",
    "usb3",
    "usb4"
};

typedef struct _PARTITION_RECORD {
    u8 status;                              /* Partition status; see above */
    u8 chs_start[3];                        /* Cylinder-head-sector address to first block of partition */
    u8 type;                                /* Partition type; see above */
    u8 chs_end[3];                          /* Cylinder-head-sector address to last block of partition */
    u32 lba_start;                          /* Local block address to first sector of partition */
    u32 block_count;                        /* Number of blocks in partition */
} __attribute__((__packed__)) PARTITION_RECORD;


typedef struct _MASTER_BOOT_RECORD {
    u8 code_area[446];                      /* Code area; normally empty */
    PARTITION_RECORD partitions[4];         /* 4 primary partitions */
    u16 signature;                          /* MBR signature; 0xAA55 */
} __attribute__((__packed__)) MASTER_BOOT_RECORD;


static int USBDevice_Init (int usbTimeout, devicesCallback cb)
	{
    time_t start = time(0);
	
	gprintf ("USBDevice_Init: begin\n");

    while (time(0) - start < usbTimeout) // 5 sec
		{
        if(storage->startup() && storage->isInserted())
            break;

		if (cb) cb();
        usleep(100000); // 1/5 sec
		}
	
    if(!storage->startup() || !storage->isInserted())
		{
		gprintf ("USBDevice_Init: device initialization failed\n");
        return -1;
		}

	gprintf ("USBDevice_Init: device initialization ok\n");

    int i;
	u8 fullsector[4096];
    MASTER_BOOT_RECORD *mbr = (MASTER_BOOT_RECORD *)fullsector;
    char BootSector[4096];

	int partnfs = 0, partfat = 0;

    storage->readSectors(0, 1, mbr);
	
	/*
	if (mounted[DEV_SD])
		fsop_WriteFile ("sd://mbr->dat", (u8*)&mbr, sizeof (mbr));
	*/
    for (i = 0; i < 4; ++i)
		{
		gprintf ("USBDevice_Init: partcount %d, parttyp = %d\n", i, mbr->partitions[i].type);
		
        if (mbr->partitions[i].type == 0)
            continue;

        storage->readSectors(le32(mbr->partitions[i].lba_start), 1, BootSector);
		/*
		if (mounted[DEV_SD])
			{
			char path[300];
			sprintf (path,"sd://part%d.dat",i);
			fsop_WriteFile (path, (u8*)BootSector, sizeof (BootSector));
			}
		*/
        if(*((u16 *) (BootSector + 0x1FE)) == 0x55AA)
			{
            //! Partition typ can be missleading the correct partition format. Stupid lazy ass Partition Editors.
            if(memcmp(BootSector + 0x36, "FAT", 3) == 0 || memcmp(BootSector + 0x52, "FAT", 3) == 0)
				{
                if (fatMount(DeviceName[DEV_USB+i], storage, le32(mbr->partitions[i].lba_start), CACHE, SECTORS))
					{
					gprintf ("USBDevice_Init: fat->updating slot %d\n", DEV_USB+i);

					partinfo[DEV_USB+i] = partfat;
					partfat++;
					mounted[DEV_USB+i] = 1;
					}
				}
            else if (memcmp(BootSector + 0x03, "NTFS", 4) == 0)
				{
				//if (ntfsMount(DeviceName[DEV_USB+i], storage, le32(mbr->partitions[i].lba_start), CACHE, SECTORS, NTFS_SHOW_HIDDEN_FILES | NTFS_RECOVER | NTFS_IGNORE_CASE))
				if (ntfsMount(DeviceName[DEV_USB+i], storage, le32(mbr->partitions[i].lba_start), CACHE, SECTORS, NTFS_SHOW_HIDDEN_FILES | NTFS_RECOVER))
					{
					gprintf ("USBDevice_Init: ntfs->updating slot %d\n", DEV_USB+i);

					partinfo[DEV_USB+i] = partnfs + 10;
					partnfs++;
					mounted[DEV_USB+i] = 1;
					}
				}
			}
		}
	
	return partnfs + partfat;

	return -1;
}

void devices_Mount (int devmode, int usbTimeout, devicesCallback cb)
	{
	memset (mounted, 0, sizeof (mounted));
	memset (partinfo, 0, sizeof (partinfo));
	
	// Start with SD
	if (fatMountSimple(DeviceName[DEV_SD], &__io_wiisd))
		{
		mounted[DEV_SD] = 1;
		}
		
	// Then mound USB
	if (usbTimeout)
		{
		if (devmode == DEVMODE_CIOSX)
			storage=(DISC_INTERFACE*)&__io_wiiums;
		else
			storage=(DISC_INTERFACE*)&__io_usbstorage;
			
		USBDevice_Init (usbTimeout, cb);
		}
	}

void devices_Unmount (void)
	{
    int dev;
    char mntName[20];

    for (dev = 0; dev < DEV_MAX; dev++)
		if (mounted[dev])
			{
			sprintf(mntName, "%s:/", DeviceName[dev]);
			fatUnmount(mntName);
			ntfsUnmount(mntName, true);
			}

	storage->shutdown();
	}

char *devices_Get (int dev)
	{
	if (!mounted[dev]) return NULL;
	return DeviceName[dev];
	}
	
int devices_PartitionInfo (int dev) // 0, 1,... FAT, 10,11.... NTFS
	{
	return partinfo[dev];
	}