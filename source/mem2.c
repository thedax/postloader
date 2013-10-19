#include <ogc/machine/asm.h>
#include <ogc/lwp_heap.h>
#include <ogc/system.h>
#include <ogc/machine/processor.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "mem2.h"
#include "debug.h"

static heap_cntrl mem2_heap;

typedef struct
	{
	char asciiId[6];// id in ascii format
	u64 titleId; 	// title id
	u32 priority;	// Vote !
	bool hidden;	// if 1, this app will be not listed
	
	u8 ios;		 	// ios to reload
	u8 vmode;	 	// 0 Default Video Mode	1 NTSC480i	2 NTSC480p	3 PAL480i	4 PAL480p	5 PAL576i	6 MPAL480i	7 MPAL480p
	s8 language; 	//	-1 Default Language	0 Japanese	1 English	2 German	3 French	4 Spanish	5 Italian	6 Dutch	7 S. Chinese	8 T. Chinese	9 Korean
	u8 vpatch;	 	// 0 No Video patches	1 Smart Video patching	2 More Video patching	3 Full Video patching
	u8 hook;	 	// 0 No Ocarina&debugger	1 Hooktype: VBI	2 Hooktype: KPAD	3 Hooktype: Joypad	4 Hooktype: GXDraw	5 Hooktype: GXFlush	6 Hooktype: OSSleepThread	7 Hooktype: AXNextFrame
	u8 ocarina; 	// 0 No Ocarina	1 Ocarina from NAND 	2 Ocarina from SD	3 Ocarina from USB"
	u8 bootMode;	// 0 Normal boot method	1 Load apploader
	u16 playcount;	// how many time this title has bin executed
	}
s_channelConfig;


u32 InitMem2Manager () 
{
	int size = (36*1024*1024);
	u32 level;
	_CPU_ISR_Disable(level);
	size &= ~0x1f; // round down, because otherwise we may exceed the area
	
	gprintf ("InitMem2Manager = 0x%0X %u 0x%0X\r\n", SYS_GetArena2Hi(), size, SYS_GetArena2Hi()-size);
	void *mem2_heap_ptr = (void *)((u32)SYS_GetArena2Hi()-size);
	SYS_SetArena2Hi(mem2_heap_ptr);
	_CPU_ISR_Restore(level);
	size = __lwp_heap_init(&mem2_heap, mem2_heap_ptr, size, 32);
	return size;
}

void* mem2_malloc(u32 size)
	{
	void *ptr;
	ptr = __lwp_heap_allocate(&mem2_heap, size);
	Debug ("mem2 allocated %u bytes", mem2_getblocksize (ptr));
	return ptr;
	}

bool mem2_free(void *ptr)
	{
	return __lwp_heap_free(&mem2_heap, ptr);
	}

static __inline__ heap_block* __lwp_heap_blockat(heap_block *block,u32 offset)
{
	return (heap_block*)((char*)block + offset);
}
 
u32 mem2_getblocksize (void *ptr)
	{
	heap_block *block;
	u32 dsize,level;

	_CPU_ISR_Disable(level);
	u32 offset = *(((u32*)ptr)-1);
	block = __lwp_heap_blockat(ptr,-offset+-HEAP_BLOCK_USED_OVERHEAD); 	
	dsize = block->front_flag&~HEAP_BLOCK_USED;
	_CPU_ISR_Restore(level);

	return dsize;
	}

void *MEM2_lo_alloc(unsigned int s)
{
	return malloc(s);
}

void MEM2_lo_free(void *p)
{
	free(p);
} 

void *MEM2_lo_realloc(void *p, unsigned int s)
{
	return realloc(p, s);
	
} 