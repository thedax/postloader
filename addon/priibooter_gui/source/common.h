// common.h

#include "../../../source/plneek.h"

#define DEVMAX 2

#define DEV_SD 0
#define DEV_USB 1

// Global functions
int fadeIn (int init) ;

// io.c
s32 Fat_Mount(int dev, int *keypressed);
void Fat_Unmount(void);

// utils.c
void printd(const char *text, ...);
u8 *ReadFile2Buffer (char *path, size_t *filesize, int *err);
bool FileExist (char *fn);
bool DirExist (char *path);
int CountObjectsInFolder(char *path);

// Global vars
extern char mount[DEVMAX][16];
