#include <gccore.h>


typedef struct _dirent
{
	char name[ISFS_MAXPATH + 1];
	int type;
} dirent_t;



s32 read_full_file_from_nand(char *filepath, u8 **buffer, u32 *filesize);
s32 read_file_from_nand(char *filepath, u8 *buffer, u32 filesize);
u8 * readalloc_file_from_nand(char *filepath, s32 *error, size_t bytes2read, size_t *bytesReaded);
s32 getdircount(char *path, u32 *cnt);
s32 getdir(char *path, dirent_t **ent, u32 *cnt);
