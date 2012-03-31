//functions for manipulating the HBC stub by giantpune

#include <gccore.h>
#include <stdlib.h>
#include <string.h>

extern const u8 stub_bin[];
extern const u32 stub_bin_size; 

static char* determineStubTIDLocation()
{
    u32 *stubID = (u32*) 0x80001818;

    //HBC stub 1.0.6 and lower, and stub.bin
    if (stubID[0] == 0x480004c1 && stubID[1] == 0x480004f5)
        return (char *) 0x800024C6;

    //HBC stub changed @ version 1.0.7.  this file was last updated for HBC 1.0.8
    else if (stubID[0] == 0x48000859 && stubID[1] == 0x4800088d) return (char *) 0x8000286A;

    //hexdump( stubID, 0x20 );
    return NULL;

}

#define TITLE_1(x)      ((u8)((x) >> 8))
#define TITLE_2(x)      ((u8)((x) >> 16))
#define TITLE_3(x)      ((u8)((x) >> 24))
#define TITLE_4(x)      ((u8)((x) >> 32))
#define TITLE_5(x)      ((u8)((x) >> 40))
#define TITLE_6(x)      ((u8)((x) >> 48))
#define TITLE_7(x)      ((u8)((x) >> 56))

s32 Set_Stub(u64 reqID)
{
    char *stub = determineStubTIDLocation();
    if (!stub) return -68;

    stub[0] = TITLE_7( reqID );
    stub[1] = TITLE_6( reqID );
    stub[8] = TITLE_5( reqID );
    stub[9] = TITLE_4( reqID );
    stub[4] = TITLE_3( reqID );
    stub[5] = TITLE_2( reqID );
    stub[12] = TITLE_1( reqID );
    stub[13] = ((u8) (reqID));

    DCFlushRange(stub, 0x10);

    return 1;

}

void StubLoad ( void )
	{
	// the offset is 3274 (0xCCA) for the first value
	
	memcpy((void*)0x80001800, stub_bin, stub_bin_size);
	
	*(vu16*)0x800024CA = 0x504F; //"PO"
	*(vu16*)0x800024D2 = 0x5354; //"ST"
	
	DCFlushRange((void*)0x80001800,stub_bin_size);
	return;	
	}

void StubUnload ( void )
{
	//some apps apparently dislike it if the stub stays in memory but for some reason isn't active :/
	memset((void*)0x80001800, 0, stub_bin_size);
	DCFlushRange((void*)0x80001800,stub_bin_size);	
	return;
}
