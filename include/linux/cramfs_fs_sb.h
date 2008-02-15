#ifndef _CRAMFS_FS_SB
#define _CRAMFS_FS_SB

/*
 * cramfs super-block data in memory
 */
 
/*
 * Portions Copyright (C) Motorola 2003
 *
 * 2003-Dec-24 - (Motorola) Added changes for linear and block cramfs
 */
 
#ifdef CONFIG_ARCH_EZX
/* Motorola -- both Linear and Block Cramfs */
struct cramfs_sb_info {
			unsigned long magic;
			unsigned long size;
			unsigned long blocks;
			unsigned long files;
			unsigned long flags;
			unsigned short l_x_b;  //Added by Motorola
			unsigned long linear_phys_addr;
			char *        linear_virt_addr;

};
#else  //original code

struct cramfs_sb_info {
			unsigned long magic;
			unsigned long size;
			unsigned long blocks;
			unsigned long files;
			unsigned long flags;
#ifdef CONFIG_CRAMFS_LINEAR
			unsigned long linear_phys_addr;
			char *        linear_virt_addr;
#endif
};

#endif

#endif
