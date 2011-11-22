#define MICROSNEEK_TOEXEC 0
#define MICROSNEEK_EXECUTED 1
#define MICROSNEEK_IOS 7 // This is the entry in ios list

typedef struct 
	{
	u64 titleId; 	// title id
	
	int status;
	
	char source[256];
	char target[256];
	}
s_microsneek;

