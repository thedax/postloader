/*

multithread

a collection of multithread safe i/o functions.

*/

#include <ogcsys.h>
#include <stdio.h>

static mutex_t mutex = LWP_THREAD_NULL;
static bool initialized = false;

void mt_Init (void)
	{
	if (mutex == LWP_THREAD_NULL) LWP_MutexInit (&mutex, false);
	initialized = true;
	}
	
void mt_Deinit (void)
	{
	if (mutex != LWP_THREAD_NULL) LWP_MutexDestroy (mutex);
	initialized = false;
	}
	
void mt_Lock (void)
	{
	if (initialized) LWP_MutexLock (mutex);
	}

void mt_Unlock (void)
	{
	if (initialized) LWP_MutexUnlock (mutex);
	}
	
FILE * mt_fopen ( const char * filename, const char * mode )
	{
	FILE *f;
	
	if (initialized) LWP_MutexLock (mutex);
	f = fopen ( filename, mode );
	if (initialized) LWP_MutexUnlock (mutex);
	
	return f;
	}
	
int mt_fclose ( FILE * stream )
	{
	int ret;
	
	if (initialized) LWP_MutexLock (mutex);
	ret = fclose ( stream );
	if (initialized) LWP_MutexUnlock (mutex);
	
	return ret;
	}
	
int mt_fseek ( FILE * stream, long int offset, int origin )
	{
	int ret;
	
	if (initialized) LWP_MutexLock (mutex);
	ret = fseek ( stream, offset, origin );
	if (initialized) LWP_MutexUnlock (mutex);
	
	return ret;
	}

long int mt_ftell ( FILE * stream )
	{
	long int ret;
	
	if (initialized) LWP_MutexLock (mutex);
	ret = ftell ( stream );
	if (initialized) LWP_MutexUnlock (mutex);
	
	return ret;
	}

size_t mt_fwrite ( const void * ptr, size_t size, size_t count, FILE * stream )
	{
	size_t ret;
	
	if (initialized) LWP_MutexLock (mutex);
	ret = fwrite ( ptr, size, count, stream );
	if (initialized) LWP_MutexUnlock (mutex);
	
	return ret;
	}

size_t mt_fread ( void * ptr, size_t size, size_t count, FILE * stream )
	{
	size_t ret;
	
	if (initialized) LWP_MutexLock (mutex);
	ret = fread ( ptr, size, count, stream );
	if (initialized) LWP_MutexUnlock (mutex);
	
	return ret;
	}
