// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#ifndef STUBBED
#define STUBBED(x)	\
	do																								\
	{																								\
		static bool AlreadySeenThisStubbedSection = false;											\
		if (!AlreadySeenThisStubbedSection)															\
		{																							\
			AlreadySeenThisStubbedSection = true;													\
			fprintf(stderr, "STUBBED: %s at %s:%d (%s)\n", x, __FILE__, __LINE__, __FUNCTION__);	\
		}																							\
	} while (0)
#endif

/*----------------------------------------------------------------------------
Metadata macros.
----------------------------------------------------------------------------*/

#define CPP       1
#define STRUCTCPP 1
#define DEFAULTS  0


/*-----------------------------------------------------------------------------
Seek-free defines.
-----------------------------------------------------------------------------*/

#define STANDALONE_SEEKFREE_SUFFIX	TEXT("_SF")


/*-----------------------------------------------------------------------------
Macros for enabling heap storage instead of inline storage on delegate types.
Can be overridden by setting to 1 or 0 in the project's .Target.cs files.
-----------------------------------------------------------------------------*/

#ifdef USE_SMALL_DELEGATES
	// UE_DEPRECATED(4.22, "USE_SMALL_DELEGATES has been removed")
	#error USE_SMALL_DELEGATES has been removed - please use NUM_DELEGATE_INLINE_BYTES instead
#else
	#ifndef NUM_DELEGATE_INLINE_BYTES
		#define NUM_DELEGATE_INLINE_BYTES 0
	#endif
#endif

#ifdef USE_SMALL_MULTICAST_DELEGATES
	// UE_DEPRECATED(4.22, "USE_SMALL_MULTICAST_DELEGATES has been removed")
	#error USE_SMALL_MULTICAST_DELEGATES has been removed - please use NUM_MULTICAST_DELEGATE_INLINE_ENTRIES instead
#else
	#ifndef NUM_MULTICAST_DELEGATE_INLINE_ENTRIES
		#define NUM_MULTICAST_DELEGATE_INLINE_ENTRIES 0
	#endif
#endif

#ifdef USE_SMALL_TFUNCTIONS
	// UE_DEPRECATED(4.22, "USE_SMALL_TFUNCTIONS has been removed")
	#error USE_SMALL_TFUNCTIONS has been removed - please use NUM_TFUNCTION_INLINE_BYTES instead
#else
	#ifndef NUM_TFUNCTION_INLINE_BYTES
		#define NUM_TFUNCTION_INLINE_BYTES 32
	#endif
#endif

#ifndef NO_CVARS
	#define NO_CVARS 0
#endif

#ifndef UE_DEPRECATE_MUTABLE_TOBJECTPTR
#define UE_DEPRECATE_MUTABLE_TOBJECTPTR 0
#endif

#ifndef UE_REFERENCE_COLLECTOR_REQUIRE_OBJECTPTR
#define UE_REFERENCE_COLLECTOR_REQUIRE_OBJECTPTR 0
#endif

#if UE_REFERENCE_COLLECTOR_REQUIRE_OBJECTPTR
#undef UE_DEPRECATE_MUTABLE_TOBJECTPTR
#define UE_DEPRECATE_MUTABLE_TOBJECTPTR 1
#endif
