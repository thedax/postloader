#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <sys/unistd.h>
#include "disc.h"

#define VER "[0.1]"
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

	Disc_WiiBoot (true, 0, NULL, 0, FALSE, FALSE, TRUE, NULL, 0, 0, 0, 0, 0, 0);
	
	exit (0);
	}
