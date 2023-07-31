// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// This file contains some hacks to solve differences between platforms
// This file also contains wrappers for all the libc methods used in the runtime.
// No other place in the runtime should include any libc header.

#include "HAL/PlatformTime.h"
#include "Logging/LogMacros.h"
#include "Stats/Stats.h"

//! There is a define MUTABLE_PROFILE that can be passed on the compiler options to enable
//! the internal profiling methods. See Config.h for more information.

#include "MuR/Types.h"
#include "HAL/UnrealMemory.h"

MUTABLERUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogMutableCore, Log, All);


//! Unify debug defines
#if !defined(MUTABLE_DEBUG)
    #if !defined(NDEBUG) || defined(_DEBUG)
        #define MUTABLE_DEBUG
    #endif
#endif


namespace mu
{

	//---------------------------------------------------------------------------------------------
	inline int mutable_vsnprintf
		( char *buffer, size_t sizeOfBuffer, const char *format, va_list argptr )
	{
		#ifdef _MSC_VER
			return vsnprintf_s( buffer, sizeOfBuffer, _TRUNCATE, format, argptr );
		#else
			return vsnprintf( buffer, sizeOfBuffer, format, argptr );
		#endif
	}


	//---------------------------------------------------------------------------------------------
	inline int mutable_snprintf
		( char *buffer, size_t sizeOfBuffer, const char *format, ... )
	{
		va_list ap;
		va_start( ap, format );
		int res = mutable_vsnprintf( buffer, sizeOfBuffer, format, ap );
		va_end(ap);
		return res;
	}


	//---------------------------------------------------------------------------------------------
	// Disgracefully halt the program.
	inline void Halt()
	{
		#ifdef _MSC_VER
            __debugbreak();
		#else
			__builtin_trap();
		#endif
	}

}
