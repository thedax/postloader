//functions for manipulating the HBC stub by giantpune

#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include "stub.h"
#include "debug.h"

/*

// This will work with fix94 stub, but it si incompatible with devolution

void StubLoad ( void )
	{
	// Clear potential homebrew channel stub
	memset((void*)0x80001800, 0, 0x1800);

	// Copy our own stub into memory
	memcpy((void*)0x80001800, stub_bin, stub_bin_size);

	// Lower Title ID
	*(vu8*)0x80001bf2 = 0x00;
	*(vu8*)0x80001bf3 = 0x01;
	*(vu8*)0x80001c06 = 0x00;
	*(vu8*)0x80001c07 = 0x01;

	// Upper Title ID
	*(vu8*)0x80001bfa = 'P';
	*(vu8*)0x80001bfb = 'O';
	*(vu8*)0x80001c0a = 'S';
	*(vu8*)0x80001c0b = 'T';
	
	DCFlushRange((void*)0x80001800, 0x1800);
	return;	
	}

void StubUnload ( void )
	{
	//some apps apparently dislike it if the stub stays in memory but for some reason isn't active :/
	memset((void*)0x80001800, 0, 0x1800);
	DCFlushRange((void*)0x80001800, 0x1800);	
	return;
	}

*/

static char* determineStubTIDLocation()
{
    u32 *stubID = (u32*) 0x80001818;

    //HBC stub 1.0.6 and lower, and stub.bin
    if (stubID[0] == 0x480004c1 && stubID[1] == 0x480004f5)
        return (char *) 0x800024C6;

    //HBC stub changed @ version 1.0.7.  this file was last updated for HBC 1.0.8
    else if (stubID[0] == 0x48000859 && stubID[1] == 0x4800088d) return (char *) 0x8000286A;

    Debug_hexdump( stubID, 0x20 );
    return NULL;

}

#define TITLE_ID(x,y)		(((u64)(x) << 32) | (y))
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
	Debug ("StubLoad()");
	
	if (Set_Stub (TITLE_ID(0x00010001,0x504f5354)) > 0)
		{
		Debug ("stub patched succesfully");
		}
	else
		{
		u8* ptr = ((void*)0x80001800);
		
		Debug ("STUB 0x%X 0x%X 0x%X 0x%X ", ptr[0x762], ptr[0x763], ptr[0x76A], ptr[0x76B]);
		    
		if (ptr[0x762] == 0x50 && ptr[0x763] == 0x4F && ptr[0x76A] == 0x53 && ptr[0x76B] == 0x54)
			{
			Debug ("stub is from hbc 1.1.0");
			
			//memcpy (ptr, stub_bin, stub_bin_size);
			ptr[0x762] = 'P';
			ptr[0x763] = 'O';
			ptr[0x76A] = 'S';
			ptr[0x76B] = 'T';
			
			DCFlushRange((void*)0x80001800, 0x1800);
			}
		else
			{
			Debug ("stub not found...");
			}
		
		}
	
	/*
	*/
	
	return;	
	}

void StubUnload ( void )
{
	Debug ("StubUnload()");
	
	//some apps apparently dislike it if the stub stays in memory but for some reason isn't active :/
	memset((void*)0x80001800, 0, 0x1800);
	DCFlushRange((void*)0x80001800, 0x1800);	
	return;
}
