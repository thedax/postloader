#define NANDCONFIG_MAXNAND 8
#define NANDCONFIG_NANDINFO_SIZE 0x100
#define NANDCONFIG_CONFIG_SIZE 0x10

#define PLNANDSTATUS_NONE 0
#define PLNANDSTATUS_BOOTING 1
#define PLNANDSTATUS_BOOTED 2

#define NEEK2O_NAND "usb://Nands/pln2o"
#define NEEK2O_SNEEK "usb://sneek"

typedef struct
{	
	u32 NandCnt;
	u32 NandSel;
	u32 Padding1;
	u32 Padding2;
	u8  NandInfo[][NANDCONFIG_NANDINFO_SIZE];
} NandConfig;

extern NandConfig *nandConfig;

s32 neek_SelectGame( u32 SlotID );

bool neek_WriteNandConfig (void);
bool neek_GetNandConfig (void);

void neek_KillDIConfig (void);	// Reboot needed after that call
void neek_KillNandConfig (void);	// Reboot needed after that call

bool neek_IsNeek2o (void);
int neek_NandConfigSelect (char *nand);

bool neek_PLNandInfoKill (void); // Remove nandcfg.pl... postloader do this when it start

bool neek_CreateCDIConfig (char *gameid); // gameid must contain ID6 value

bool neek_PrepareNandForChannel (char *sneekpath, char *nandpath);
bool neek_RestoreNandForChannel (char *sneekpath);

bool neek_PLNandInfo (int mode, u32 *idx, u32 *status, u32 *lo, u32 *hi, u32 *back2real); // mode 0 = read, mode 1 = write;

bool neek_UID_Dump (void);
bool neek_UID_Read (void);
bool neek_UID_Write (void);
int neek_UID_Find (u64 title_id);
bool neek_UID_Remove (u64 title_id);
bool neek_UID_Add (u64 title_id);
int neek_UID_Count (void);
int neek_UID_CountHidden (void);
int neek_UID_Clean (void); // return the number of erased items
bool neek_UID_Hide (u64 title_id);
bool neek_UID_Show (u64 title_id);
bool neek_UID_IsHidden (u64 title_id);