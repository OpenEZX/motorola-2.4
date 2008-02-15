/*
 * Defines some debug macros for smbfs.
 */

/* This makes a dentry parent/child name pair. Useful for debugging printk's */
#define DENTRY_PATH(dentry) \
	(dentry)->d_parent->d_name.name,(dentry)->d_name.name

/*
 * safety checks that should never happen ??? 
 * these are normally enabled.
 */
#ifdef SMBFS_PARANOIA
#define PARANOIA(x...) {printk(KERN_NOTICE "%s: ", __FUNCTION__);	\
			printk(x);}
#else
#define PARANOIA(x...) do { ; } while(0)
#endif

/* lots of debug messages */
#ifdef SMBFS_DEBUG_VERBOSE
#define VERBOSE(x...) {printk(KERN_DEBUG "%s: ", __FUNCTION__);		\
			printk(x);}
#else
#define VERBOSE(x...) do { ; } while(0)
#endif

/*
 * "normal" debug messages, but not with a normal DEBUG define ... way
 * too common name.
 */
#ifdef SMBFS_DEBUG
#define DEBUG1(x...) {printk(KERN_DEBUG "%s: ", __FUNCTION__);		\
			printk(x);}
#else
#define DEBUG1(x...) do { ; } while(0)
#endif
