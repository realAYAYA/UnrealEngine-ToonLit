// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"
#include "Debugapi.h"

#if _DEBUG

/**
 * TextureShare Debug Log
 */
namespace TextureShareDebugLog
{
	static constexpr int32 MaxDebugLogLineLen = 1024;
	static wchar_t DebugLogLine[MaxDebugLogLineLen];

	static auto constexpr EOL = TEXT("\r\n");
	static int32 constexpr LengthEOL = 3;
};

#define DEBUG_LOG(Format,...)\
	{\
		std::swprintf(TextureShareDebugLog::DebugLogLine, TextureShareDebugLog::MaxDebugLogLineLen, Format, ##__VA_ARGS__); \
		int32 StrLen = (int32)std::wcslen(TextureShareDebugLog::DebugLogLine);\
		for(size_t Index = 0; Index<TextureShareDebugLog::LengthEOL; Index++)\
		{\
			TextureShareDebugLog::DebugLogLine[StrLen + Index] = TextureShareDebugLog::EOL[Index];\
		}\
		OutputDebugStringW(TextureShareDebugLog::DebugLogLine);\
	}

#else

// No log in release build
#define DEBUG_LOG(Format,...)

#endif
