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
#include "wdvd.h"

#define VER "[0.5]"
#define BC 0x0000000100000100ULL
static tikview view ATTRIBUTE_ALIGN(32);

//void green_fix(void); //GREENSCREEN FIX

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
	rr = Disc_IsGC();
	Debug("Disc_IsGC() returned: %d", rr);
	
	if (rr == 0)
		{
		rr = WDVD_ReadDiskId ((void*)0x80000000);
		Debug("WDVD_ReadDiskId() returned: %d", rr);
		
		rr = WDVD_EnableAudio(*(u8*)0x80000008);
		Debug("WDVD_EnableAudio() returned: %d", rr);
		
		*(volatile unsigned int *)0xCC003024 |= 7;

		int retval = ES_GetTicketViews(BC, &view, 1);

		if (retval != 0)
			{
			Debug("ES_GetTicketViews fail %d", retval);
			exit (0);
			}
			
		retval = ES_LaunchTitle(BC, &view);
		exit (0);
		}
		
	// Check disc 
	rr = Disc_IsWii();
	Debug("Disc_IsWii() returned: %d", rr);

	if (rr == 0)
		{
		// Read header 
		rr = Disc_ReadHeader(header);
		Debug("Disc_ReadHeader() returned: %d", rr);

		Disc_WiiBoot (0, FALSE, TRUE, 0);
		}
	
	exit (0);
	}
