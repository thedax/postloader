#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>

#define HAVE_AHBPROT 	((*(vu32*)0xcd800064 == 0xFFFFFFFF) ? 1 : 0)
#define MEM2_PROT    	0x0D8B420A
#define ES_MODULE_START (u16*)0x939F0000
#define HW_DIFLAGS 		0x0D800180

static const u16 ticket_check[] = {
    0x685B,          // ldr r3,[r3,#4] ; get TMD pointer
    0x22EC, 0x0052,  // movls r2, 0x1D8
    0x189B,          // adds r3, r3, r2; add offset of access rights field in TMD
    0x681B,          // ldr r3, [r3]   ; load access rights (haxxme!)
    0x4698,          // mov r8, r3     ; store it for the DVD video bitcheck later
    0x07DB           // lsls r3, r3, #31; check AHBPROT bit
};

int PatchAHB (void)
	{
    if (HAVE_AHBPROT)
		{
        u16 *patchme;
        
		// Disable memory protection
		write16(MEM2_PROT, 2);
		
        for (patchme=ES_MODULE_START; patchme < ES_MODULE_START+0x4000; patchme++) 
			{
            if (!memcmp(patchme, ticket_check, sizeof(ticket_check))) 
				{
                // write16/uncached poke doesn't work for this. Go figure.
                patchme[4] = 0x23FF; // li r3, 0xFF
                DCFlushRange(patchme+4, 2);
                break;
				}
			}
		
		return 1;
		}
	else
		return 0;
    }

int ios_ReloadIOS(int ios, int *ahbprot)
	{
    int ret;
	
	#ifdef DOLPHINE
	return 0;
	#endif
	
	if (ahbprot != NULL) *ahbprot = 0;
	
	if (ios > 200) //mmm, just reload it
		{
		IOS_ReloadIOS(ios);
		return ios;
		}
	
	if (ios < 0 || ios > 254) // -1 can be passed to auto choose the preferred
		{
		ios = IOS_GetPreferredVersion();
		
		if (ios <= 9) // mmm... maybe something gones wrong
			ios = 58;
		}
	
    /* Enable AHBPROT on title launch */
	ret = PatchAHB();
    if (ret) 
		{
        /* Reload current IOS, typically IOS58 */
        IOS_ReloadIOS(ios);
		
		if (HAVE_AHBPROT) 
			{
			if (ahbprot != NULL) *ahbprot = 1;

			// Disable memory protection
			write16(MEM2_PROT, 2);
			mask32(HW_DIFLAGS, 0x200000, 0);			
			}

		return ios; // Wow, it's works
		}
		
	// mmm... anyway re-load, but ahpbrot
	IOS_ReloadIOS(ios);
		
	return 0;
    }