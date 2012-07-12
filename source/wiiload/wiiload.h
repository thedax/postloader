#define WIILOAD_ZIPFILE "wiiload.zip"
#define WIILOAD_TMPFILE "wiiload.tmp"

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
	int gecko;
	
	char filename[64];	// Filename sended by wiiload protocolo
	char fullpath[256];	// full path to temp file on device
	
	char *args;
	int argl;
	}
s_wiiload;

extern s_wiiload wiiload;

void WiiLoad_Start(char *tempPath, int startSleep);
void WiiLoad_Stop (void);
void WiiLoad_Pause (void);
void WiiLoad_Resume (void);