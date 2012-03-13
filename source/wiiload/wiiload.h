#define WIILOAD_ZIPFILE "wiiload.zip"

enum 
	{
	WIILOAD_ERROR = -1,
	WIILOAD_STOPPED = 0,	// Thread stopped
	WIILOAD_INITIALIZING,
	WIILOAD_IDLE,
	WIILOAD_RECEIVING,		// Wiiload thread is receiving
	WIILOAD_HBREADY,		// Homebrew loaded in execute buffer
	WIILOAD_HBZREADY,		// wiiload.zip written in temp folder and ready to use
	};

typedef struct 
	{
	char op[128];		// Calling process can set filename or any other info that fit
	u32 opUpd;			// Every time op is updated, opUpd is incremented
	int status;
	
	char filename[32];	// Filename of the dol/elf sended
	
	u8 *buff;
	int buffsize;
	
	char *args;
	int argl;
	}
s_wiiload;

extern s_wiiload wiiload;

void WiiLoad_Start(char *tempPath, int startSleep);
void WiiLoad_Stop (void);
void WiiLoad_Pause (void);
void WiiLoad_Resume (void);