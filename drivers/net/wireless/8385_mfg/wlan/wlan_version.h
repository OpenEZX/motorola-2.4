/*
 * 	 version.h 
 *
 */

#include "../release_version.h"
#include <if_version.h>
#include <bus_version.h>

#define COMMON_VERSION	"40"

const char driver_version[]  = 
#ifdef SD8305
	"sd8305-%s-" SD8305_DRIVER_RELEASE_VERSION
#endif

#ifdef SD8385
	"sd8385"
#ifdef RF8031
	"h"
#endif
	"-%s-" SD8385_DRIVER_RELEASE_VERSION
#endif

#ifdef SPI8385
	"spi8385"
#ifdef RF8031
	"h"
#endif
	"-%s-" SPI8385_DRIVER_RELEASE_VERSION
#endif

#ifdef SPI8381
	"spi8381"
#ifdef RF8015
	"pn"
#endif
	"-%s-" SPI8381_DRIVER_RELEASE_VERSION
#endif

#ifdef GSPI8385
	"gspi8385-%s-" GSPI8385_DRIVER_RELEASE_VERSION
#endif

#ifdef CF8305
	"COMM-CF8305-%s-" CF8305_DRIVER_RELEASE_VERSION
#endif

#ifdef CF8381
	"COMM-CF8381"
#ifdef RF8015
	"PN"
#endif
#ifdef RF8010
	"P"
#endif
	"-%s-" CF8381_DRIVER_RELEASE_VERSION
#endif

#ifdef CF8385
	"COMM-CF8385"
#ifdef RF8031
	"H"
#endif
	"-%s-" CF8385_DRIVER_RELEASE_VERSION
#endif

#ifdef MS8380
	"COMM-MS8380-%s-" MS8380_DRIVER_RELEASE_VERSION
#endif

#ifdef MS8381
	"COMM-MS8381-%s-" MS8381_DRIVER_RELEASE_VERSION
#endif

#ifdef MS8385
	"COMM-MS8385-%s-" MS8385_DRIVER_RELEASE_VERSION
#endif
	
#ifdef USB83xx
#ifdef MIPS
	"COMM-USB8388-%s-" USB8388_MIPS_DRIVER_RELEASE_VERSION
#else
	"COMM-USB8388-%s-" USB8388_DRIVER_RELEASE_VERSION
#endif
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
