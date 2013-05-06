#ifndef _MULTITHREAD_
	#define _MULTITHREAD_

	void mt_Init (void);
	void mt_Deinit (void);
	void mt_Lock (void);
	void mt_Unlock (void);
	FILE * mt_fopen ( const char * filename, const char * mode );
	int mt_fclose ( FILE * stream );
	int mt_fseek ( FILE * stream, long int offset, int origin );
	long int mt_ftell ( FILE * stream );
	size_t mt_fwrite ( const void * ptr, size_t size, size_t count, FILE * stream );
	size_t mt_fread ( void * ptr, size_t size, size_t count, FILE * stream );
#endif