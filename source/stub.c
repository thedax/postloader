//functions for manipulating the HBC stub by giantpune

#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include "stub.h"

extern const u8 stub_bin[];
extern const u32 stub_bin_size; 

void StubLoad ( void )
	{
	/* Clear potential homebrew channel stub */
	memset((void*)0x80001800, 0, 0x1800);

	/* Copy our own stub into memory */
	memcpy((void*)0x80001800, stub_bin, stub_bin_size);

	/* Lower Title ID */
	*(vu8*)0x80001bf2 = 0x00;
	*(vu8*)0x80001bf3 = 0x01;
	*(vu8*)0x80001c06 = 0x00;
	*(vu8*)0x80001c07 = 0x01;

	/* Upper Title ID */
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
