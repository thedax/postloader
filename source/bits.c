#include <ogcsys.h>
#include "debug.h"
#include "bits.h"

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

void bits_SetInArray (size_t bit, bool value, u8 *buff)
	{
	size_t index = bit >> 3; // bit/8
	size_t bindex = bit - (index << 3); // index*8
	
	bitWrite (buff[index], bindex, value);
	/*
	bool b = bitRead (buff[index], bindex);
	Debug ("bit %d (%d %d) = %d (%d)", bit, index, bindex, value, b);
	*/
	}
	
bool bits_GetFromArray (size_t bit, u8 *buff)
	{
	size_t index = bit >> 3; // bit/8
	size_t bindex = bit - (index << 3); // index*8
	
	return bitRead (buff[index], bindex);
	}
