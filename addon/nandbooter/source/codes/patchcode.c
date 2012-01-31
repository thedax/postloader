#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <sys/unistd.h>

#include "patchcode.h"
#include "codes.h"

extern void patchhook(u32 address, u32 len);
extern void patchhook2(u32 address, u32 len);
extern void patchhook3(u32 address, u32 len);

extern u32 ocarinaoption;
extern u32 debuggeroption;
extern u32 hooktypeoption;

extern void multidolhook(u32 address);

static const u32 multidolpatch1[2] = {
	0x3C03FFB4,0x28004F43
};	

static const u32 multidolpatch2[2] = {
	0x3F608000, 0x807B0018
};

const u32 viwiihooks[4] = {
	0x7CE33B78,0x38870034,0x38A70038,0x38C7004C 
};

const u32 kpadhooks[4] = {
	0x9A3F005E,0x38AE0080,0x389FFFFC,0x7E0903A6
};

const u32 kpadoldhooks[6] = {
	0x801D0060, 0x901E0060, 0x801D0064, 0x901E0064, 0x801D0068, 0x901E0068
};

const u32 joypadhooks[4] = {
	0x3AB50001, 0x3A73000C, 0x2C150004, 0x3B18000C
};

const u32 gxdrawhooks[4] = {
	0x3CA0CC01, 0x38000061, 0x3C804500, 0x98058000
};

const u32 gxflushhooks[4] = {
	0x90010014, 0x800305FC, 0x2C000000, 0x41820008
};

const u32 ossleepthreadhooks[4] = {
	0x90A402E0, 0x806502E4, 0x908502E4, 0x2C030000
};

const u32 axnextframehooks[4] = {
	0x3800000E, 0x7FE3FB78, 0xB0050000, 0x38800080
};

const u32 wpadbuttonsdownhooks[4] = {
	0x7D6B4A14, 0x816B0010, 0x7D635B78, 0x4E800020
};

const u32 wpadbuttonsdown2hooks[4] = {
	0x7D6B4A14, 0x800B0010, 0x7C030378, 0x4E800020
};

const u32 multidolchanhooks[4] = {
	0x4200FFF4, 0x48000004, 0x38800000, 0x4E800020
};

//---------------------------------------------------------------------------------
bool dochannelhooks(void *addr, u32 len, bool bootcontentloaded)
//---------------------------------------------------------------------------------
{
	void *addr_start = addr;
	void *addr_end = addr+len;
	bool patched = false;
	bool multidolpatched = false;
	
	while(addr_start < addr_end)
	{
		switch(hooktypeoption)
		{
				
			case 0x00:	
				
				break;
				
			case 0x01:	
				if(memcmp(addr_start, viwiihooks, sizeof(viwiihooks))==0)
				{
					patchhook((u32)addr_start, len);
					patched = true;
				}
				break;
				
			case 0x02:
				
				if(memcmp(addr_start, kpadhooks, sizeof(kpadhooks))==0)
				{
					patchhook((u32)addr_start, len);
					patched = true;
				}
				
				if(memcmp(addr_start, kpadoldhooks, sizeof(kpadoldhooks))==0)
				{
					patchhook((u32)addr_start, len);
					patched = true;
				}
				break;
				
			case 0x03:
				
				if(memcmp(addr_start, joypadhooks, sizeof(joypadhooks))==0)
				{
					patchhook((u32)addr_start, len);
					patched = true;
				}
				break;
				
			case 0x04:
				
				if(memcmp(addr_start, gxdrawhooks, sizeof(gxdrawhooks))==0)
				{
					patchhook((u32)addr_start, len);
					patched = true;
				}
				break;
				
			case 0x05:
				
				if(memcmp(addr_start, gxflushhooks, sizeof(gxflushhooks))==0)
				{
					patchhook((u32)addr_start, len);
					patched = true;
				}
				break;
				
			case 0x06:
				
				if(memcmp(addr_start, ossleepthreadhooks, sizeof(ossleepthreadhooks))==0)
				{
					patchhook((u32)addr_start, len);
					patched = true;
				}
				break;
				
			case 0x07:
				
				if(memcmp(addr_start, axnextframehooks, sizeof(axnextframehooks))==0)
				{
					patchhook((u32)addr_start, len);
					patched = true;
				}
				break;
				
			case 0x08:
				
				//if(memcmp(addr_start, customhook, customhooksize)==0){
				//	patchhook((u32)addr_start, len);
				//patched = true;
				//}
				break;
		}
		if (hooktypeoption != 0)
		{
			if(memcmp(addr_start, multidolchanhooks, sizeof(multidolchanhooks))==0)
			{
				*(((u32*)addr_start)+1) = 0x7FE802A6;
				DCFlushRange(((u32*)addr_start)+1, 4);
				ICInvalidateRange(((u32*)addr_start)+1, 4);

				multidolhook((u32)addr_start+sizeof(multidolchanhooks)-4);
				multidolpatched = true;
			}
		}
		
		addr_start += 4;
	}
	
	if (bootcontentloaded)
	{
		return multidolpatched;
	} else
	{
		return patched;
	}
}