#include <stdarg.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <ntfs.h>
#include <dirent.h>
#include "usbstorage.h"

#include "globals.h"

//these are the only stable and speed is good
#define CACHE 8
#define SECTORS 64

const DISC_INTERFACE* storage = NULL;

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
static int mounted[MAXDEVICES] = {0}; // Part num, or part num + 10 for ntfs

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

    while (time(0) - start < 5) // 5 sec
    {
        if(storage->startup() && storage->isInserted())
            break;

        usleep(200000); // 1/5 sec
    }

    if(!storage->startup() || !storage->isInserted())
        return -1;

    int i;
    MASTER_BOOT_RECORD mbr;
    char BootSector[512];

    storage->readSectors(0, 1, &mbr);
	
	int partnfs = 0, partfat = 0;

    for(i = 0; i < 4; ++i)
    {
        if(mbr.partitions[i].type == 0)
            continue;

        storage->readSectors(le32(mbr.partitions[i].lba_start), 1, BootSector);

        if(*((u16 *) (BootSector + 0x1FE)) == 0x55AA)
        {
            //! Partition typ can be missleading the correct partition format. Stupid lazy ass Partition Editors.
            if(memcmp(BootSector + 0x36, "FAT", 3) == 0 || memcmp(BootSector + 0x52, "FAT", 3) == 0)
            {
                if (fatMount(DeviceName[USB1+i], storage, le32(mbr.partitions[i].lba_start), CACHE, SECTORS))
					{
					partinfo[USB1+i] = partfat;
					partfat++;
					mounted[USB1+i] = 1;
					}
            }
            else if (memcmp(BootSector + 0x03, "NTFS", 4) == 0)
            {
				if (ntfsMount(DeviceName[USB1+i], storage, le32(mbr.partitions[i].lba_start), CACHE, SECTORS, NTFS_SHOW_HIDDEN_FILES | NTFS_RECOVER | NTFS_IGNORE_CASE))
					{
					partinfo[USB1+i] = partnfs + 10;
					partnfs++;
					mounted[USB1+i] = 1;
					}
            }
        }
    }
	
	return partnfs + partfat;

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
	storage->shutdown();
	USB_Deinitialize();
}

#define BUFFSIZE (1024*64)
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
	char buff[128];
    char gi[GISIZE];
	int i;

	pdir=opendir(path);
	while ((pent=readdir(pdir)) != NULL) 
		{
		sprintf (fn, "%s/%s", path, pent->d_name);
		
		if (strlen (ob) > BUFFSIZE - 64)
			{
			Debug ("ScanWBFS: too many entryes");
			break;
			}
		
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
			
			char *p = ob;
			
			// Add title
			gi[0x20 + 64] = 0;		// Make sure to not oveflow
			strcpy (buff, &gi[0x20]);
			for (i = 0; i < strlen(buff);i++) if (buff[i] < 32 || i > 125) {buff[i] = 0;break;}			
			if (strlen(buff) == 0) *p = 0;
			strcat (ob, buff);
			strcat (ob, "\1");
			
			// Add id
			gi[0x00 + 6] = 0;		// Make sure to not oveflow
			strcpy (buff, &gi[0x0]);
			for (i = 0; i < strlen(buff);i++) if (buff[i] < 32 || i > 125) {buff[i] = 0;break;}
			if (strlen(buff) == 0) *p = 0;
			strcat (ob, buff);
			strcat (ob, "\1");
			
			// Add partition
			sprintf (tmp, "%d", part);
			strcpy (buff, tmp);
			for (i = 0; i < strlen(buff);i++) if (buff[i] < 32 || i > 125) {buff[i] = 0;break;}
			if (strlen(buff) == 0) *p = 0;
			strcat (ob, buff);
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
	
	memset (&partinfo, 0, sizeof(partinfo)); // Part num, or part num + 10 for ntfs
	memset (&mounted, 0, sizeof(mounted));
	
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
		snd_Pause ();
		
		Fat_Unmount ();
		
		// Let's remount sd, required for debugging
		Debug ("WBFSSCanner: mounting sd for debugging");
		fatMountSimple("sd", &__io_wiisd);

		if (vars.ios == IOS_DEFAULT)
			storage=(DISC_INTERFACE*)&__io_wiiums;
		else
			storage=(DISC_INTERFACE*)&__io_usbstorage;
		
		// Mount every partitions on usb
		Debug ("WBFSSCanner: mounting devs");
		USBDevice_Init ();
		
		Debug ("WBFSSCanner: scannning");
		int i;
		for (i = USB1; i <= USB4; i++)
			{
			if (mounted[i])
				{
				part = partinfo[i];
				char path[64];
				
				sprintf (path, "usb%d://wbfs", i);
				ScanWBFS (ob, path);
				}
			}
		
		Debug ("WBFSSCanner: unomount usb");
		USBDevice_deInit ();
		
		Debug ("WBFSSCanner: unomount sd");
		fatUnmount("sd:/");
		__io_wiisd.shutdown();

		Debug ("WBFSSCanner: remounting standard devs");
		MountDevices (1);
		
		Debug ("WBFSSCanner: writing cache file");
		f = fopen (path, "wb");
		if (f) 
			{
			fwrite (ob, 1, strlen(ob)+1, f);
			fclose (f);
			}
		
		snd_Resume ();
		}
	
	// Adjust the ob
	
	Debug ("WBFSSCanner: adjust ob");
	
	int i, l;
	l = strlen(ob);
	for (i = 0; i < l; i++)
		if (ob[i] == '\1')
			ob[i] = '\0';
	
	Debug ("WBFSSCanner: adjust ob");
	
	return ob;
	}
