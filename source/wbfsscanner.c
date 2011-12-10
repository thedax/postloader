#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <ntfs.h>
#include <dirent.h>

#include "globals.h"

//these are the only stable and speed is good
#define CACHE 8
#define SECTORS 64

#define le32(i) (((((u32) i) & 0xFF) << 24) | ((((u32) i) & 0xFF00) << 8) | \
                ((((u32) i) & 0xFF0000) >> 8) | ((((u32) i) & 0xFF000000) >> 24))

enum
{
    SD = 0,
    USB1,
    USB2,
    USB3,
    USB4,
    MAXDEVICES
};

static int partinfo[MAXDEVICES] = {0}; // Part num, or part num + 10 for ntfs

static const char DeviceName[MAXDEVICES][6] =
{
    "sd",
    "usb1",
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

static int USBDevice_Init()
{
    time_t start = time(0);

    while(start-time(0) < 10) // 10 sec
    {
        if(__io_usbstorage.startup() && __io_usbstorage.isInserted())
            break;

        usleep(200000); // 1/5 sec
    }

    if(!__io_usbstorage.startup() || !__io_usbstorage.isInserted())
        return -1;

    int i;
    MASTER_BOOT_RECORD mbr;
    char BootSector[512];

    __io_usbstorage.readSectors(0, 1, &mbr);
	
	int partnfs = 0, partfat = 0;

    for(i = 0; i < 4; ++i)
    {
        if(mbr.partitions[i].type == 0)
            continue;

        __io_usbstorage.readSectors(le32(mbr.partitions[i].lba_start), 1, BootSector);

        if(*((u16 *) (BootSector + 0x1FE)) == 0x55AA)
        {
            //! Partition typ can be missleading the correct partition format. Stupid lazy ass Partition Editors.
            if(memcmp(BootSector + 0x36, "FAT", 3) == 0 || memcmp(BootSector + 0x52, "FAT", 3) == 0)
            {
                fatMount(DeviceName[USB1+i], &__io_usbstorage, le32(mbr.partitions[i].lba_start), CACHE, SECTORS);
				partinfo[i] = partfat;
				partfat++;
            }
            else if (memcmp(BootSector + 0x03, "NTFS", 4) == 0)
            {
                ntfsMount(DeviceName[USB1+i], &__io_usbstorage, le32(mbr.partitions[i].lba_start), CACHE, SECTORS, NTFS_SHOW_HIDDEN_FILES | NTFS_RECOVER | NTFS_IGNORE_CASE);
				partinfo[i] = partnfs + 10;
				partnfs++;
            }
        }
    }

	return -1;
}

static void USBDevice_deInit()
{
    int dev;
    char Name[20];

    for(dev = USB1; dev <= USB4; ++dev)
    {
        sprintf(Name, "%s:/", DeviceName[dev]);
        fatUnmount(Name);
        ntfsUnmount(Name, true);
        //ext2Unmount(Name);
    }
	//Let's not shutdown so it stays awake for the application
	__io_usbstorage.shutdown();
	USB_Deinitialize();
}

#define BUFFSIZE (1024*16)
#define GISIZE 0xEC
static int count = 0;
static int part = 0;

bool ScanWBFS (char *ob, char *path)
	{
	DIR *pdir;
	struct dirent *pent;
	FILE* f = NULL;
	char fn[128];
	char tmp[128];
    char gi[GISIZE];

	pdir=opendir(path);
	while ((pent=readdir(pdir)) != NULL) 
		{
		sprintf (fn, "%s/%s", path, pent->d_name);
		
		// Let's check if it is a folder
		if (strcmp (pent->d_name, ".") != 0 && strcmp (pent->d_name, "..") != 0 && fsop_DirExist (fn))
			{
			// recurse it
			ScanWBFS (ob, fn);
			}
		else if (strstr (pent->d_name, ".wbfs") || strstr (pent->d_name, ".WBFS"))
			{
			strcpy (tmp, pent->d_name);
			tmp[24] = '\0';
			Video_WaitPanel (TEX_HGL, "%d|%s", ++count, tmp);
			
			Debug ("ScanWBFS: [FN] %s", fn);
			
			f = fopen(fn, "rb");
			if (!f) continue;
			fseek (f, 0x200, 0);
			fread( gi, 1, GISIZE, f);
			
			// Add title
			strcat (ob, &gi[0x20]);
			strcat (ob, "\1");
			// Add id
			strcat (ob, &gi[0x0]);
			strcat (ob, "\1");
			// Add partition
			sprintf (tmp, "%d", part);
			strcat (ob, tmp);
			strcat (ob, "\1");
			
			fclose (f);
			}
		}
	closedir(pdir);
	
	return TRUE;
	}

char * WBFSSCanner (bool reset)
	{
	char path[300];
	FILE *f;
	count = 0;
	char *ob = calloc (1, BUFFSIZE);
	
	sprintf (path, "%s://ploader/wbfs.cfg", vars.defMount);
	
	if (reset == 0)
		{
		f = fopen (path, "rb");
		if (!f) 
			reset = 1;
		else
			{
			fread (ob, 1, BUFFSIZE-1, f);
			fclose (f);
			
			ob[BUFFSIZE-1] = 0;
			}
		}
	
	if (reset)
		{
		Video_WaitPanel (TEX_HGL, "Please wait...|(mounting partitions)");

		Fat_Unmount ();

		// Mount every partitions on usb
		USBDevice_Init ();
		
		part = partinfo[0];
		ScanWBFS (ob, "usb1://wbfs");
		part = partinfo[1];
		ScanWBFS (ob, "usb2://wbfs");
		part = partinfo[2];;
		ScanWBFS (ob, "usb3://wbfs");
		part = partinfo[3];
		ScanWBFS (ob, "usb4://wbfs");
		
		USBDevice_deInit ();

		MountDevices (1);

		f = fopen (path, "wb");
		if (f) 
			{
			fwrite (ob, 1, strlen(ob)+1, f);
			fclose (f);
			}
		}
	
	// Adjust the ob
	int i, l;
	l = strlen(ob);
	for (i = 0; i < l; i++)
		if (ob[i] == '\1')
			ob[i] = '\0';
	
	return ob;
	}
