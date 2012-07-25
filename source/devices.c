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
#include <errno.h>

//these are the only stable and speed is good
#define CACHE 8
#define SECTORS 64

const DISC_INTERFACE* storage = NULL;

#define le32(i) (((((u32) i) & 0xFF) << 24) | ((((u32) i) & 0xFF00) << 8) | \
                ((((u32) i) & 0xFF0000) >> 8) | ((((u32) i) & 0xFF000000) >> 24))

static int partinfo[DEV_MAX] = {0}; // Part num, or part num + 10 for ntfs
static int mounted[DEV_MAX] = {0}; // Part num, or part num + 10 for ntfs

static int partnfs = 0, partfat = 0;

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

static int isFAT32 (char *BootSector)
	{
	if (memcmp(BootSector + 0x36, "FAT", 3) == 0 || memcmp(BootSector + 0x52, "FAT", 3) == 0)
		return true;
		
	return false;
	}

static int isNTFS (char *BootSector)
	{
	if (memcmp(BootSector + 0x03, "NTFS", 4) == 0)
		return true;
		
	return false;
	}

static int USBDevice_Init (int usbTimeout, devicesCallback cb)
	{
    time_t start = time(0);
	
	gprintf ("USBDevice_Init: begin\n");

    while (time(0) - start < usbTimeout) // 5 sec
		{
        if(storage->startup() && storage->isInserted())
            break;

		if (cb) 
			{
			if (cb() == 0) break;
			}
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

    storage->readSectors(0, 1, mbr);
	
	/*
	if (mounted[DEV_SD])
		fsop_WriteFile ("sd://mbr->dat", (u8*)&mbr, sizeof (mbr));
	*/
	
	int j;
	int inc;
	int idx;
	
	// Let's check if the firs part is ntfs... in this case we expect a fat32 next
	storage->readSectors(le32(mbr->partitions[0].lba_start), 1, BootSector);
	
	if (isNTFS (BootSector))
		{
		i = 3;
		inc = -1;
		}
	else
		{
		i = 0;
		inc = 1;
		}
	
	idx = 0;
    for (j = 0; j < 4; ++j)
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
            if(isFAT32 (BootSector))
				{
                if (fatMount(DeviceName[DEV_USB+idx], storage, le32(mbr->partitions[i].lba_start), CACHE, SECTORS))
					{
					gprintf ("USBDevice_Init: fat->updating slot %d\n", DEV_USB+i);

					partinfo[DEV_USB+idx] = partfat;
					partfat++;
					mounted[DEV_USB+idx] = 1;
					idx ++;
					}
				else
					{
					gprintf ("USBDevice_Init: ntfsMount->unable to mount partition (%d, %s)\n", errno, DeviceName[DEV_USB+i]);
					}
				}
            else if (isNTFS (BootSector))
				{
				//ntfsRemoveDevice(DeviceName[DEV_USB+idx]);
				
				if (ntfsMount(DeviceName[DEV_USB+idx], storage, le32(mbr->partitions[i].lba_start), CACHE, SECTORS, NTFS_SHOW_HIDDEN_FILES | NTFS_RECOVER))
					{
					gprintf ("USBDevice_Init: ntfs->updating slot %d\n", DEV_USB+i);

					partinfo[DEV_USB+idx] = partnfs + 10;
					partnfs++;
					mounted[DEV_USB+idx] = 1;
					
					idx++;
					}
				else
					{
					gprintf ("USBDevice_Init: ntfsMount->unable to mount partition (%d, %s)\n", errno, DeviceName[DEV_USB+i]);
					}
				}
			}
		else
			{
			gprintf ("USBDevice_Init: wrong signature 0x%04X\n", *((u16 *) (BootSector + 0x1FE)));
			}

		i += inc;
		}
	
	return partnfs + partfat;

	return -1;
}

void devices_Mount (int devmode, int neek, int usbTimeout, devicesCallback cb)
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
			{
			gprintf ("using wiiums\r\n");
			storage = (DISC_INTERFACE*)&__io_usbstorage2_port1;
			}
		else
			{
			gprintf ("using usbstorage\r\n");
			storage=(DISC_INTERFACE*)&__io_usbstorage;
			}
		
		if (neek == 0)
			{
			USBDevice_Init (usbTimeout, cb);
			}
		else
			{
			if (fatMountSimple(DeviceName[DEV_USB], &__io_usbstorage))
				{
				mounted[DEV_USB] = 1;
				}
			}
		}
	}

void devices_Unmount (void)
	{
    int dev;
    char mntName[20];

    for (dev = 0; dev < DEV_MAX; dev++)
		{
		if (mounted[dev])
			{
			sprintf(mntName, "%s:/", DeviceName[dev]);
			
			if (partinfo[dev] < 10)
				fatUnmount(mntName);
			else
				ntfsUnmount(mntName, true);
			}
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
	
// This function can be used to tick the usb device
// to prevent it to go in sleep
int devices_TickUSB (void)
	{
	int ret = 0;
	u8 sector[4096];
	static time_t t = 0;
	
	if (storage && (partnfs || partfat) && time(NULL) > t)
		{
		ret = storage->readSectors(0, 1, sector);
		t = time(NULL) + 30;
		
		gprintf ("USB ticked \n");
		}
		
	return ret;
	}