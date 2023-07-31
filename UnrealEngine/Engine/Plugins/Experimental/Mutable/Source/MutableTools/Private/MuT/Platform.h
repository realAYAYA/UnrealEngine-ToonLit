// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Platform.h"
#include "MuR/MemoryPrivate.h"

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <sys/stat.h>


// This file contains some hacks to solve differences between platforms necessary for the tools
// library and not present in the run-time library.


namespace mu
{


	//! STL-like containers using this allocator
	template< typename T >
	using vector = std::vector<T>;

	template< typename T >
	using basic_string = std::basic_string<T, std::char_traits<T>>;

	using string = std::basic_string<char, std::char_traits<char> >;

	template< typename K, typename T >
	using map = std::map< K, T, std::less<K> >;

	template< typename T >
	using set = std::set< T, std::less<T> >;

	template< typename T >
	using multiset = std::multiset< T, std::less<T> >;

	template< typename K, typename T >
	using pair = std::pair<K, T>;

    //-------------------------------------------------------------------------------------------------
    inline FILE* mutable_fopen
		(
			const char *filename,
			const char *mode
		)
	{
		FILE* pFile = 0;

#ifdef _MSC_VER
        errno_t error = fopen_s( &pFile, filename, mode );
        if (error!=0)
        {
            // It's valid to fail, just return null.
            //check(false);
            pFile = nullptr;
        }
#else
		pFile = fopen( filename, mode );
#endif

		return pFile;
	}


    //-------------------------------------------------------------------------------------------------
    inline int64_t mutable_ftell( FILE* f )
    {
#ifdef _MSC_VER
        // Windows normal ftell works only with 32 bit int sizes
        return _ftelli64(f);
#else
        return ftell(f);
#endif
    }


    //-------------------------------------------------------------------------------------------------
    inline int64_t mutable_fseek( FILE* f, int64_t pos, int origin )
    {
#ifdef _MSC_VER
        // Windows normal fseek works only with 32 bit int sizes
        return _fseeki64(f,pos,origin);
#else
        return fseek(f,pos,origin);
#endif
    }


}

