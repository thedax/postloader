//functions for manipulating the HBC stub by giantpune

#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include "stub.h"
#include "debug.h"
#include "globals.h"

#define STUBSIZE 0x1800

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

static u8 *stub = NULL;

void StubGet (void)
	{
	char mex[500];
	char path[256];
	size_t read;

	sprintf (path, "%s://ploader/stub.bin", vars.defMount);
	
	stub = fsop_ReadFile (path, STUBSIZE, &read);
	
	if (stub && read == STUBSIZE)
		{
		Debug ("%s loaded succesfully", path);
		return;
		}
	
	sprintf (mex, "stub.bin not found\n\n"
					"postLoader need to save current HBC stub\n"
					"Make sure you have started postLoader\n"
					"from genuine HomeBrewChannel\n"
					"If you have executed postloader from priiloader\n"
					"or other loader, press 'skip'\n\n"
					"Do not forget to install postLoader forwarder\n"
					"channel to have return to postLoader working.");
	
	int item = grlib_menu ( mex, "Ok, dump it!~Skip");
	
	if (item == 0)
		{
		DCFlushRange((void*)0x80001800, STUBSIZE);
		
		if (fsop_WriteFile (path, ((u8 *) 0x80001800), STUBSIZE))
			{
			Debug ("stub saved succesfully");
			grlib_menu ( "stub saved succesfully", "  Ok  ");
			}
		}
	}

static char* determineStubTIDLocation()
{
    u32 *stubID = (u32*) 0x80001818;

    if (stubID[0] == 0x480004c1 && stubID[1] == 0x480004f5)
		{
		//HBC stub 1.0.6 and lower, and stub.bin
		
        return (char *) 0x800024C6;
		}
    else if (stubID[0] == 0x48000859 && stubID[1] == 0x4800088d) 
		{
		//HBC stub changed @ version 1.0.7.  this file was last updated for HBC 1.0.8
		
		return (char *) 0x8000286A;
		}
    else if (stubID[0] == 0x00000000 && stubID[1] == 0x00000000) 
		{
		//HBC stub changed @ version 1.1.0
		
		return (char *) 0x8000286A;
		}
		
	Debug ("stubID[0] = 0x%08X, stubID[1] = 0x%08X",  stubID[0],  stubID[1]);

    // Debug_hexdump( stubID, 0x20 );
    return NULL;

}

#define TITLE_0(x)      ((u8)(x))
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

    stub[0]  = TITLE_7( reqID );
    stub[1]  = TITLE_6( reqID );
    stub[8]  = TITLE_5( reqID );
    stub[9]  = TITLE_4( reqID );
    stub[4]  = TITLE_3( reqID );
    stub[5]  = TITLE_2( reqID );
    stub[12] = TITLE_1( reqID );
    stub[13] = TITLE_0( reqID );

    DCFlushRange(stub, 0x10);

    return 1;
	}

void StubLoad ( void )
	{
	Debug ("StubLoad()");
	
	memcpy((void*)0x80001800, stub, STUBSIZE);
	
	if (Set_Stub (TITLE_ID(0x00010001,0x504f5354)) > 0)
		{
		Debug ("stub patched succesfully");
		}
	else
		{
		u8* ptr = ((void*)0x80001800);
		
		Debug ("STUB 0x%X 0x%X 0x%X 0x%X ", ptr[0x762], ptr[0x763], ptr[0x76A], ptr[0x76B]);
		    
		if (ptr[0x762] == 0xAF && ptr[0x763] == 0x1B && ptr[0x76A] == 0xF5 && ptr[0x76B] == 0x16)
			{
			Debug ("stub is from hbc 1.1.0");
			
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

	return;	
	}

void StubUnload ( void )
{
	Debug ("StubUnload()");
	
	// Simple clear the STUBHAXX... some homebrew will try to call it
	
	*(u32*)0x80001804 = (u32) 0L;
	*(u32*)0x80001808 = (u32) 0L;
	DCFlushRange((void*)0x80001804,8);

	return;
}
