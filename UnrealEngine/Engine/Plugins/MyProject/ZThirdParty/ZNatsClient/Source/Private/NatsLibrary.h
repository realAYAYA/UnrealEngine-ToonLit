#pragma once

#if PLATFORM_WINDOWS
#	include "Windows/AllowWindowsPlatformTypes.h"
#	include "Windows/AllowWindowsPlatformAtomics.h"
#endif
THIRD_PARTY_INCLUDES_START
// ==================================================================
//

#if PLATFORM_WINDOWS
#	include <winsock2.h>
#	pragma comment(lib, "ws2_32.lib")
#endif

#include "nats/nats.h"



//
// ==================================================================
THIRD_PARTY_INCLUDES_END
#if PLATFORM_WINDOWS
#	include "Windows/HideWindowsPlatformAtomics.h"
#	include "Windows/HideWindowsPlatformTypes.h"
#endif
