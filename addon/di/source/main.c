#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <network.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <sys/unistd.h>
#include "disc.h"
#include "patchcode.h"
#include "debug.h"

#define VER "[0.4]"
#define POSTLOADER_SD "sd://postloader.dol"
#define POSTLOADER_USB "usb://postloader.dol"
#define PRII_WII_MENU 0x50756E65

void green_fix(void); //GREENSCREEN FIX

//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
	{
	VIDEO_Init();
	VIDEO_SetBlack(true);  // Disable video output during initialisation

	net_wc24cleanup();
	
	if (fatMountSimple("sd", &__io_wiisd))
		DebugStart (true, "sd://ploader.log");
	else
		DebugStart (true, NULL);

	configbytes[0] = 0xCD;
	//configbytes[0] = 0;

	Debug ("---------------------------------------------------------------------------");
	Debug ("                             di "VER" by stfour");
	Debug ("                       (part of postLoader project)");
	Debug ("---------------------------------------------------------------------------");
	
	struct discHdr *header;
	header = (struct discHdr *)memalign(32, sizeof(struct discHdr));

	s32 rr = Disc_Init();
	Debug("Disc_Init() returned: %d", rr);
	
	rr = Disc_Open();
	Debug("Disc_Open() returned: %d", rr);

	// Check disc 
	rr = Disc_IsWii();
	Debug("Disc_IsWii() returned: %d", rr);

	// Read header 
	rr = Disc_ReadHeader(header);
	Debug("Disc_ReadHeader() returned: %d", rr);

	//green_fix ();
	Disc_WiiBoot (0, FALSE, TRUE, 0);
	//Disc_WiiBoot (0, TRUE, TRUE, 0);
	
	exit (0);
	}
