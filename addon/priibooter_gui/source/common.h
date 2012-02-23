// common.h

#include "../../../source/plneek.h"

// Global functions
int fadeIn (int init) ;

// utils.c
void printd(const char *text, ...);
u8 *ReadFile2Buffer (char *path, size_t *filesize, int *err);
bool FileExist (char *fn);
bool DirExist (char *path);
int CountObjectsInFolder(char *path);
