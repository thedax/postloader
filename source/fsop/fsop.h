#ifndef _FSOP
#define _FSOP

typedef void (*fsopCallback)(void); 

typedef struct 
	{
	u64 size, bytes; // for operation that uses more than one file
	time_t start_t;
	}
s_fsopmulty;

typedef struct 
	{
	char op[256];	// Calling process can set filename or any other info that fit
	
	u32 size, bytes;

	s_fsopmulty multy;
	
	int flag1;		// user defined flag
	bool breakop;	// allow to stop a long operation
	}
s_fsop;

extern s_fsop fsop;

bool fsop_StoreBuffer (char *fn, u8 *buff, int size, fsopCallback vc);
bool fsop_FileExist (char *fn);
bool fsop_DirExist (char *path);
bool fsop_CopyFile (char *source, char *target, fsopCallback vc);
int fsop_MakeFolder (char *path);
bool fsop_CopyFolder (char *source, char *target, fsopCallback vc);
bool fsop_KillFolderTree (char *source, fsopCallback vc);
bool fsop_CreateFolderTree (char *path);
int fsop_CountFolderTree (char *path);

u32 fsop_GetFolderKb (char *source, fsopCallback vc);
u64 fsop_GetFolderBytes (char *source, fsopCallback vc);
u32 fsop_GetFreeSpaceKb (char *path);

#endif