#define NANDCONFIG_MAXNAND 8
#define NANDCONFIG_NANDINFO_SIZE 0xC0
#define NANDCONFIG_CONFIG_SIZE 0x10

#define PLNANDSTATUS_NONE 0
#define PLNANDSTATUS_BOOTING 1
#define PLNANDSTATUS_BOOTED 2


typedef struct
{	
	u32 NandCnt;
	u32 NandSel;
	u32 Padding1;
	u32 Padding2;
	u8  NandInfo[][NANDCONFIG_NANDINFO_SIZE];
} NandConfig;

extern NandConfig *nandConfig;

bool neek_WriteNandConfig (void);
bool neek_GetNandConfig (void);

void neek_KillDIConfig (void);	// Reboot needed after that call
void neek_KillNandConfig (void);	// Reboot needed after that call

bool neek_IsNeek2o (void);
bool neek_NandConfigSelect (char *nand);

bool neek_PLNandInfo (int mode, u32 *idx, u32 *status, u32 *lo, u32 *hi, u32 *back2real); // mode 0 = read, mode 1 = write;
bool neek_PLNandInfoRemove (void); // mode 0 = read, mode 1 = write