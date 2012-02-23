#ifndef __PLNEEK__
	#define __PLNEEK__

#define NAND_MAX 16

#define PLNEEK_SDDIR "sd://ploader/plneek.dir"
#define PLNEEK_USBDIR "usb://ploader/plneek.dir"

#define PLNEEK_SDDAT "sd://ploader/plneek.dat"
#define PLNEEK_USBDAT "usb://ploader/plneek.dat"

#define PLN_BOOT_REAL 0
#define PLN_BOOT_NEEK 1
#define PLN_BOOT_SM 2
#define PLN_BOOT_HBC 3
#define PLN_BOOT_NEEK2O 4

#define PLN_NAME_LEN 64

// types
typedef struct 
	{
	char path[64];	// full path to nand
	int count;		// num of objects inside
	}
s_plnNandFolders;

typedef struct 
	{
	char name[PLN_NAME_LEN]; 	// this is the name
	unsigned char bootMode;

	s_plnNandFolders nands[NAND_MAX];
	int nandsCnt;
	}
s_plneek;

#endif