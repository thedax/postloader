#define SBIA bits_SetInArray
#define GBFA bits_GetFromArray

void bits_SetInArray (size_t bit, bool value, u8 *buff);
bool bits_GetFromArray (size_t bit, u8 *buff);