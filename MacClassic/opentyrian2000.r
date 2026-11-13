/*
 * Resources for the Retro68 (classic Mac OS, System 7 - Mac OS 9) build.
 * Used instead of Retro68's Retro68APPL.r / RetroPPCAPPL.r templates so
 * that the application gets a large enough memory partition for SDL3.
 *
 * Rez -DOPENTYRIAN_PPC=1 for the PowerPC (CFM/PEF) build.
 */

#include "Processes.r"
#include "MacTypes.r"

#ifdef OPENTYRIAN_PPC
#include "CodeFragments.r"

resource 'cfrg' (0) {
	{
		kPowerPCCFragArch, kIsCompleteCFrag, kNoVersionNum, kNoVersionNum,
		kDefaultStackSize, kNoAppSubFolder,
		kApplicationCFrag, kDataForkCFragLocator, kZeroOffset, kCFragGoesToEOF,
		"OpenTyrian2000"
	}
};
#endif

resource 'vers' (1) {
	0x01, 0x00, release, 0x00,
	verUS,
	"1.0",
	"OpenTyrian2000 for classic Mac OS"
};

/*
 * SDL3 needs far more than Retro68's default 1 MB partition: the game
 * data, the 320x200 game surface, the scaled XRGB8888 window framebuffer
 * and the audio buffers. Users can still change this in the Finder's
 * Get Info window.
 */
resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	isHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	PREFERRED_SIZE,
	MINIMUM_SIZE
};
