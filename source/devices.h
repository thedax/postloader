#define DEV_SD 0
#define DEV_USB 1
#define DEV_USB1 2
#define DEV_USB2 3
#define DEV_USB3 4
#define DEV_MAX 5

#define DEVMODE_IOS 0
#define DEVMODE_CIOSX 1

// This is the devices callback. Return 0 to interrupt the process
typedef int (*devicesCallback)(void); 

void devices_Mount (int devmode, int usbTimeout, devicesCallback cb);
void devices_Unmount (void);
char *devices_Get (int dev);
int devices_PartitionInfo (int dev);