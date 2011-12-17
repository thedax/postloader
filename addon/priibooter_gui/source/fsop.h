#ifndef _FSOP
#define _FSOP

enum
	{
	FSOP_OK = 0,
	FSOP_FOPEN,
	FSOP_FILEISEMPTY,
	FSOP_CANNOTMALLOC
	};

typedef void (*fsopCallback)(void); 

typedef struct 
	{
	char op[64];	// Calling process can set filename or any other info that fit
	int size;
	int bytes;
	
	int flag1;		// user defined flag
	}
s_fsop;

extern s_fsop fsop;

u8 *fsop_GetBuffer (char *fn, int *size, fsopCallback vc);
bool fsop_StoreBuffer (char *fn, u8 *buff, int size, fsopCallback vc);
bool fsop_FileExist (char *fn);
bool fsop_DirExist (char *path);
bool fsop_CopyFile (char *source, char *target, fsopCallback vc);
int fsop_MakeFolder (char *path);
bool fsop_CopyFolder (char *source, char *target, fsopCallback vc);
bool fsop_KillFolderTree (char *source, fsopCallback vc);
bool fsop_CreateFolderTree (char *path);
int fsop_CountFolderTree (char *path);

#endif