// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AddonTools.h"

#include <vector>

BEGIN_NAMESPACE_UE_AC

#if PLATFORM_WINDOWS

// Throw a runtime_error for last windows error
void ThrowWinError(DWORD winErr, const utf8_t* InFile, int InLineNo);

	// This define throw an std::runtime_error
	#define AvWinError()                                   \
		{                                                  \
			DWORD winErr = GetLastError();                 \
			if (winErr != 0)                               \
			{                                              \
				ThrowWinError(winErr, __FILE__, __LINE__); \
			}                                              \
		}

// String conversions utf8_t vs wchar_t
std::wstring Utf8ToUtf16(const utf8_t* s);
utf8_string	 Utf16ToUtf8(const wchar_t* s);

void SetThreadName(const char* InName);

#endif

typedef std::vector< utf8_string > VecStrings;
VecStrings						   GetPrefLanguages();

// Return the user app support directory
GS::UniString GetApplicationSupportDirectory();

// Return the user home directory
GS::UniString GetHomeDirectory();

END_NAMESPACE_UE_AC
