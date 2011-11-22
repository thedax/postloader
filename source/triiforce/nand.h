#ifndef _NAND_H_
#define _NAND_H_

/* 'NAND Device' structure */
typedef struct {
	/* Device name */
	char *name;

	/* Mode value */
	u32 mode;

	/* Un/mount command */
	u32 mountCmd;
	u32 umountCmd;
} nandDevice; 


#define EMU_SD 1
#define EMU_USB 2

/* Prototypes */
s32 Nand_Mount(nandDevice *);
s32 Nand_Unmount(nandDevice *);
s32 Nand_Enable(nandDevice *dev, char *path);
s32 Nand_Disable(void);
s32 Enable_Emu(int selection, char *path);
s32 Disable_Emu();
s32 get_nand_device();

#endif
