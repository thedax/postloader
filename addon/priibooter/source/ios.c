#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>

#define HAVE_AHBPROT ((*(vu32*)0xcd800064 == 0xFFFFFFFF) ? 1 : 0)
#define MEM_REG_BASE 0xd8b4000
#define MEM_PROT (MEM_REG_BASE + 0x20a)

#define CHECK_AHB       0x0D800064
#define MEM2_PROT       0x0D8B420A
#define ES_MODULE_START (u16*)0x939F0000

int ios_ReloadIOS(int ios, int *ahbprot);

static const u16 ticket_check[] = {
    0x685B,          // ldr r3,[r3,#4] ; get TMD pointer
    0x22EC, 0x0052,  // movls r2, 0x1D8
    0x189B,          // adds r3, r3, r2; add offset of access rights field in TMD
    0x681B,          // ldr r3, [r3]   ; load access rights (haxxme!)
    0x4698,          // mov r8, r3     ; store it for the DVD video bitcheck later
    0x07DB           // lsls r3, r3, #31; check AHBPROT bit
};

static void disable_memory_protection() 
	{
    write32(MEM_PROT, read32(MEM_PROT) & 0x0000FFFF);
	}


int PatchAHB (void)
	{
    if (HAVE_AHBPROT)
		{
        u16 *patchme;
        
		// Disable memory protection
		write16(MEM2_PROT, 2);
		//disable_memory_protection ();
		
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

static u32 apply_patch(char *name, const u8 *pattern, u32 pattern_size, const u8 *patch, u32 patch_size, u32 patch_offset) 
	{
    u8 *ptr_start = (u8*)*((u32*)0x80003134), *ptr_end = (u8*)0x94000000;
    u32 found = 0;
    u8 *location = NULL;
    while (ptr_start < (ptr_end - patch_size)) 
		{
        if (!memcmp(ptr_start, pattern, pattern_size)) 
			{
            found++;
            location = ptr_start + patch_offset;
            u8 *start = location;
            u32 i;
            for (i = 0; i < patch_size; i++) 
				{
                *location++ = patch[i];
				}
            DCFlushRange((u8 *)(((u32)start) >> 5 << 5), (patch_size >> 5 << 5) + 64);
            ICInvalidateRange((u8 *)(((u32)start) >> 5 << 5), (patch_size >> 5 << 5) + 64);
			}
        ptr_start++;
		}
    return found;
	}

const u8 es_set_ahbprot_pattern[] = { 0x68, 0x5B, 0x22, 0xEC, 0x00, 0x52, 0x18, 0x9B, 0x68, 0x1B, 0x46, 0x98, 0x07, 0xDB };
const u8 es_set_ahbprot_patch[]   = { 0x01 };

u32 IOSPATCH_AHBPROT() 
	{
    if (HAVE_AHBPROT) 
		{
        disable_memory_protection();
        return apply_patch("es_set_ahbprot", es_set_ahbprot_pattern, sizeof(es_set_ahbprot_pattern), es_set_ahbprot_patch, sizeof(es_set_ahbprot_patch), 25);
		}
    return 0;
	}

int ios_ReloadIOS(int ios, int *ahbprot)
	{
    int ret;
	
	if (ahbprot != NULL) *ahbprot = 0;
	
	if (ios > 200) //mmm, just reload it
		{
		IOS_ReloadIOS(ios);
		return ios;//IOS_GetVersion();
		}
	
	if (ios < 0 || ios > 254) // -1 can be passed to auto choose the preferred
		{
		ios = IOS_GetPreferredVersion();
		
		// mmm... maybe something gones wrong
		if (ios <= 9)
			ios = 58;
		}
	
    /* Enable AHBPROT on title launch */
    //ret = IOSPATCH_AHBPROT();
	ret = PatchAHB();
    if (ret) 
		{
        /* Reload current IOS, typically IOS58 */
        IOS_ReloadIOS(ios);
		
		if (HAVE_AHBPROT && ahbprot != NULL) *ahbprot = 1;

		return ios; // Wow, it's works
		}
		
	// mmm... anyway re-load, but ahpbrot is lost for sure
	IOS_ReloadIOS(ios);
		
	return ios;
    }

void ReloadIOS (int os)
	{
#ifndef DOLPHINE
	if (os == -1)
		{
		s32 version = IOS_GetVersion();
		s32 preferred = IOS_GetPreferredVersion();

		if (preferred > 0)
			IOS_ReloadIOS(preferred);
		else
			IOS_ReloadIOS(version);
		}
	else
		IOS_ReloadIOS(os);
#endif
	}

