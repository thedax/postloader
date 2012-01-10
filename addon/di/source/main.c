#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <network.h>
#include <wiiuse/wpad.h>
#include <sys/unistd.h>
#include "disc.h"
#include "patchcode.h"

#define VER "[0.3]"
#define POSTLOADER_SD "sd://postloader.dol"
#define POSTLOADER_USB "usb://postloader.dol"
#define PRII_WII_MENU 0x50756E65

void InitVideo (void);
void printd(const char *text, ...);

void green_fix(void); //GREENSCREEN FIX

//---------------------------------------------------------------------------------
int main(int argc, char **argv) 
	{
	//InitVideo ();
	
	VIDEO_Init();
	VIDEO_SetBlack(true);  // Disable video output during initialisation

	net_wc24cleanup();

	configbytes[0] = 0xCD;
	//configbytes[0] = 0;

	printd ("\n");
	printd ("---------------------------------------------------------------------------\n");
	printd ("                             di "VER" by stfour\n");
	printd ("                       (part of postLoader project)\n");
	printd ("---------------------------------------------------------------------------\n");
	
	struct discHdr *header;
	header = (struct discHdr *)memalign(32, sizeof(struct discHdr));

	s32 rr = Disc_Init();
	printd("Disc_Init() returned: %d\n", rr);
	
	rr = Disc_Open();
	printd("Disc_Open() returned: %d\n", rr);

	// Check disc 
	rr = Disc_IsWii();
	printd("Disc_IsWii() returned: %d\n", rr);

	// Read header 
	rr = Disc_ReadHeader(header);
	printd("Disc_ReadHeader() returned: %d\n", rr);

	//green_fix ();
	Disc_WiiBoot (0, FALSE, TRUE, 0);
	//Disc_WiiBoot (0, TRUE, TRUE, 0);
	
	exit (0);
	}
