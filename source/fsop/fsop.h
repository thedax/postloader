#ifndef _FSOP
#define _FSOP

typedef void (*fsopCallback)(void); 

typedef struct 
	{
	u64 size, bytes; // for operation that uses more than one file
	u32 startms;
	u32 elapsed;
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

char * fsop_GetExtension (char *path);
char * fsop_GetFilename (char *path, bool killExt);
char * fsop_GetPath (char *path, int killDev);
char * fsop_GetDev (char *path);

u8 *fsop_ReadFile (char *path, size_t bytes2read, size_t *bytesReaded);
bool fsop_WriteFile (char *path, u8 *buff, size_t len);
u32 fsop_CountDirItems (char *source);
bool fsop_GetFileSizeBytes (char *path, size_t *filesize);	// for me stats st_size report always 0 :(
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

char * fsop_GetDirAsString (char *path, char sep, int skipfolders, char *ext);

#endif