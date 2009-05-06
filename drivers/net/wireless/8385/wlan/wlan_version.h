/*
 * 	 version.h 
 *
 */

#include "../release_version.h"
#include <if_version.h>
#include <bus_version.h>

#define COMMON_VERSION	"86"

const char driver_version[]  = 
#ifdef SD8305
	"sd8305-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef SD8385
	"sd8385"
#ifdef RF8031
	"h"
#endif
	"-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef SD8388
	"sd8388"
#ifdef RF8031
	"h"
#endif
	"-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef SD8381
	"sd8381"
	"-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef SD8399
	"sd8399"
	"-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef SPI8385
	"spi8385"
#ifdef RF8031
	"h"
#endif
	"-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef SPI8381
	"spi8381"
#ifdef RF8015
	"pn"
#endif
	"-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef GSPI8385
	"gspi8385-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef GSPI8399
	"gspi8399-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef CF8305
	"COMM-CF8305-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef CF8381
	"COMM-CF8381"
#ifdef RF8015
	"PN"
#endif
#ifdef RF8010
	"P"
#endif
	"-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef CF8385
	"COMM-CF8385"
#ifdef RF8031
	"H"
#endif
	"-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef CF8399
	"COMM-CF8399-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef MS8380
	"COMM-MS8380-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef MS8381
	"COMM-MS8381-%s-" DRIVER_RELEASE_VERSION
#endif

#ifdef MS8385
	"COMM-MS8385-%s-" DRIVER_RELEASE_VERSION
#endif
	
#ifdef USB83xx
	"COMM-USB8388-%s-" DRIVER_RELEASE_VERSION
#endif
	
	"-("
	BUS_VERSION "."
	IF_VERSION "."
	COMMON_VERSION
	") "

#ifdef	DEBUG_VER
	"-dbg"
#endif
	;
