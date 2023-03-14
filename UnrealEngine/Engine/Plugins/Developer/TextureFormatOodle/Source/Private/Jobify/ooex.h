// Copyright Epic Games, Inc. All Rights Reserved.
#ifndef __OOEX_H__
#define __OOEX_H__

/*******

OOEX - some helpers for me to use in the examples

*******/

#ifdef _MSC_VER
#pragma warning(disable: 4127) // conditional is constant
#pragma warning(disable: 4996) // secure deprecated
#endif

// include oodle2x.h before me if you want it
//	if not, I stub in simple implementations here :
#ifndef __OODLE2X_H_INCLUDED__
#define OodleX_DisplayAssertion(file,line,func,str)	 ( fprintf(stderr,"ASSERT: %s (%d) : %s\n",file,line,str) , 1 )
#define OodleXLog_Printf_v1 printf
#define OodleXLog_Printf_v0 printf
#endif

//===========================================================

#ifndef OOEX_BREAK
#ifdef _MSC_VER
#define OOEX_BREAK()	__debugbreak()
#elif defined(__GNUC__)
#define OOEX_BREAK()	__builtin_trap()
#else
// crappy fallback BREAK
#include <stdlib.h>
#define OOEX_BREAK()	assert(0)
#endif
#endif

//===========================================================

// Minimalist platform detection for desktop platforms

#ifdef ANDROID
	#define OOEX_PLATFORM_ANDROID
#endif

#if defined(__linux__) && !defined(ANDROID)
	#define OOEX_PLATFORM_LINUX
#endif

#if defined(_WIN32) || defined(WINAPI_FAMILY)
	#ifdef WINAPI_FAMILY
		// If this is #defined, we might be in a Windows Store App. But
		// VC++ by default #defines this to a symbolic name, not an integer
		// value, and those names are defined in "winapifamily.h". So if
		// WINAPI_FAMILY is #defined, #include the header so we can parse it.
		#include <winapifamily.h>
		#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
			#define OOEX_PLATFORM_NT
		#endif
	#else // assume Windows-style platform without WINAPI_FAMILY defined is an old desktop Windows SDK
		#define OOEX_PLATFORM_NT
	#endif
#endif

#if defined(__APPLE__)
	#include "TargetConditionals.h"
	// TARGET_OS_MAC is always set on Darwin-derived platforms, even for iOS-derived targets
	#if TARGET_OS_SIMULATOR
		#define OOEX_PLATFORM_IOS
		#define OOEX_PLATFORM_IOS_SIM
	#elif TARGET_OS_IPHONE
		#define OOEX_PLATFORM_IOS
	#else
		#define OOEX_PLATFORM_MAC
	#endif
#endif

//=================================================================
// OOEX_ASSERT :

// toggle by setting OOEX_DO_ASSERTS

#ifndef OOEX_DO_ASSERTS

#if (defined(_DEBUG) && !defined(NDEBUG)) || defined(ASSERT_IN_RELEASE)
  #define OOEX_DO_ASSERTS 1
#else
  #define OOEX_DO_ASSERTS 0
#endif

#endif

//-----------------------------------------------------------

#if defined(__GNUG__) || defined(__GNUC__) || (defined(_MSC_VER) && _MSC_VER > 1200)
  #define OOEX_FUNCTION_NAME __FUNCTION__
#else
  #define OOEX_FUNCTION_NAME 0

  // __func__ is in the C99 standard
#endif

//-----------------------------------------------------------
	  
#define OOEX_ASSERT_BREAK()	  OOEX_BREAK()

#define OOEX_ASSERT_FAILURE_ALWAYS(str)			  do { if ( OodleX_DisplayAssertion(__FILE__,__LINE__,OOEX_FUNCTION_NAME,str) ) OOEX_ASSERT_BREAK(); } while(0)

#define OOEX_ASSERT_ALWAYS(exp)		 do { if ( ! (exp) ) { OOEX_ASSERT_FAILURE_ALWAYS(#exp); } } while(0)

//-----------------------------------
#if OOEX_DO_ASSERTS 

#define OOEX_ASSERT(exp)		   OOEX_ASSERT_ALWAYS(exp)
// OOEX_DURING_ASSERT is to set up expressions or declare variables that are only used in asserts
#define OOEX_DURING_ASSERT(exp)	  exp

#define OOEX_ASSERT_FAILURE(str)  OOEX_ASSERT_FAILURE_ALWAYS(str)

#else // OOEX_DO_ASSERTS //-----------------------------------

#define OOEX_ASSERT(exp)		   (void)0

#define OOEX_DURING_ASSERT(exp)	   (void)0

#define OOEX_ASSERT_FAILURE(str)   (void)0

#endif // OOEX_DO_ASSERTS //-----------------------------------

//===========================================================

// Must be placed after variable declarations for code compiled as .c
#if defined(_MSC_VER) && _MSC_VER >= 1600 // in 2010 aka 10.0 and later 
#	define OOEX_UNUSED_VARIABLE(x) x
#else
#	define OOEX_UNUSED_VARIABLE(x) (void)(sizeof(x))
#endif

//===========================================================

#ifndef OOEX_MIN
#define OOEX_MIN(a,b)	 ( (a) < (b) ? (a) : (b) )
#endif

#ifndef OOEX_MAX
#define OOEX_MAX(a,b)	 ( (a) > (b) ? (a) : (b) )
#endif

#ifndef OOEX_ABS
#define OOEX_ABS(a)		 ( ((a) < 0) ? -(a) : (a) )
#endif

#ifndef OOEX_CLAMP
#define OOEX_CLAMP(val,lo,hi) OOEX_MAX( OOEX_MIN(val,hi), lo )
#endif

//===========================================================

#define OOEX_STRINGIZE( L )			#L

#define OOEX_STRING_JOIN	OO_STRING_JOIN

//===========================================================

#ifndef OOEX_GET32_LE

// Little-Endian 32-bit integer read/write
static inline OO_U32 ooex_get32_le(const OO_U8 * ptr)
{
	OO_U32 x = ptr[0];
	x |= (OO_U32)ptr[1] << 8;
	x |= (OO_U32)ptr[2] << 16;
	x |= (OO_U32)ptr[3] << 24;
	return x;
}

static inline void ooex_put32_le(OO_U8 * ptr, OO_U32 val)
{
	ptr[0] = (OO_U8)((val >>  0) & 0xff);
	ptr[1] = (OO_U8)((val >>  8) & 0xff);
	ptr[2] = (OO_U8)((val >> 16) & 0xff);
	ptr[3] = (OO_U8)((val >> 24) & 0xff);
}

#define OOEX_GET32_LE(ptr) ooex_get32_le((const OO_U8 *)(ptr))
#define OOEX_PUT32_LE(ptr,val) ooex_put32_le((OO_U8 *)(ptr), (val))

#endif // OOEX_GET32_LE

//===========================================================

#ifdef _MSC_VER
#define OOEX_64_FMT "%I64"
#define OOEX_64_FMT_SIZE	"I64"
#else
#define OOEX_64_FMT "%ll"
#define OOEX_64_FMT_SIZE	"ll"
#endif

#define OOEX_S64_FMT	OOEX_64_FMT "d"
#define OOEX_U64_FMT	OOEX_64_FMT "u"

//===========================================================

#endif // __OOEX_H__
