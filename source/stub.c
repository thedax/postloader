//Load the open source stub 

#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include "stub.h"
#include "debug.h"
#include "globals.h"

extern const u8 stub_bin[];
extern const u32 stub_bin_size;

void StubLoad ( void )
	{
	// Clear potential homebrew channel stub
	memset((void*)0x80001800, 0, 0x1800);

	// Copy our own stub into memory
	memcpy((void*)0x80001800, stub_bin, stub_bin_size);

	DCFlushRange((void*)0x80001800, 0x1800);
	
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
