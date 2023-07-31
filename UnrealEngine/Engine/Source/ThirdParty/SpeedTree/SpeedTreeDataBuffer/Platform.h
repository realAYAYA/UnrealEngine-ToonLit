///////////////////////////////////////////////////////////////////////
//
//  *** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with
//  the terms of that agreement.
//
//      Copyright (c) 2003-2017 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com


///////////////////////////////////////////////////////////////////////
//  Preprocessor

#pragma once
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>


///////////////////////////////////////////////////////////////////////
// Packing
// certain SpeedTree data structures require particular packing, so we set it
// explicitly. Comment this line out to override, but be sure to set packing to 4 before
// including Core.h or other key header files.

#define ST_SETS_PACKING_INTERNALLY
#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////
//  Inline macros

#ifndef ST_INLINE
	#define ST_INLINE inline
#endif
#ifndef ST_FORCE_INLINE
	#ifdef NDEBUG
		#ifdef __GNUC__
			#define ST_FORCE_INLINE inline __attribute__ ((always_inline))
		#else
			#define ST_FORCE_INLINE __forceinline
		#endif
	#else
		#define ST_FORCE_INLINE inline
	#endif
#endif


////////////////////////////////////////////////////////////
//  Unreferenced parameters

#define ST_UNREF_PARAM(x) (void)(x)


///////////////////////////////////////////////////////////////////////
//  Code Safety

#define ST_PREVENT_INSTNATIATION(a) private: a();
#define ST_PREVENT_COPY(a) private: a(const a&); a& operator=(const a&);


////////////////////////////////////////////////////////////
//  Compile-time assertion

#define ST_ASSERT_ON_COMPILE(expr) extern char AssertOnCompile[(expr) ? 1 : -1]


///////////////////////////////////////////////////////////////////////
//  All SpeedTree DataBuffer classes and variables are under the namespace "SpeedTreeDataBuffer"

namespace SpeedTreeDataBuffer
{
	///////////////////////////////////////////////////////////////////////
	//  Types

	typedef bool            st_bool;
	typedef char            st_int8;
	typedef char            st_char;
	typedef wchar_t			st_wchar;
	typedef short           st_int16;
	typedef int             st_int32;
	typedef long long       st_int64;
	typedef unsigned char   st_uint8;
	typedef unsigned char   st_byte;
	typedef unsigned char   st_uchar;
	typedef unsigned short  st_uint16;
	typedef unsigned int    st_uint32;
	typedef float           st_float32;
	typedef double          st_float64;


	///////////////////////////////////////////////////////////////////////
	//  class st_float16 (half-float)

	class ST_DLL_LINK st_float16
	{
	public:
					st_float16( );
					st_float16(st_float32 fSinglePrecision);
					st_float16(const st_float16& hfCopy);

					operator st_float32(void) const;

	private:
		st_uint16   m_uiValue;
	};


	///////////////////////////////////////////////////////////////////////
	//  Compile time checks on types

	ST_ASSERT_ON_COMPILE(sizeof(st_int8) == 1);
	ST_ASSERT_ON_COMPILE(sizeof(st_int16) == 2);
	ST_ASSERT_ON_COMPILE(sizeof(st_int32) == 4);
	ST_ASSERT_ON_COMPILE(sizeof(st_int64) == 8);
	ST_ASSERT_ON_COMPILE(sizeof(st_uint8) == 1);
	ST_ASSERT_ON_COMPILE(sizeof(st_uint16) == 2);
	ST_ASSERT_ON_COMPILE(sizeof(st_uint32) == 4);
	ST_ASSERT_ON_COMPILE(sizeof(st_float16) == 2);
	ST_ASSERT_ON_COMPILE(sizeof(st_float32) == 4);
	ST_ASSERT_ON_COMPILE(sizeof(st_float64) == 8);


	///////////////////////////////////////////////////////////////////////
	//  Special SpeedTree assertion macro

	#ifndef NDEBUG
		#define st_assert(condition, explanation) assert((condition) && (explanation))
	#else
		#define st_assert(condition, explanation)
	#endif


	///////////////////////////////////////////////////////////////////////
	//  SDK-wide constants

	#if defined(_XBOX)
		static const st_char c_chFolderSeparator = '\\';
		static const st_char* c_szFolderSeparator = "\\";
	#else
		// strange way to specify this because GCC issues warnings about these being unused in Core
		static const st_char* c_szFolderSeparator = "/";
		static const st_char c_chFolderSeparator = c_szFolderSeparator[0];
	#endif


	// include inline functions
	#include "Platform_inl.h"

} // end namespace SpeedTreeDataBuffer

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

