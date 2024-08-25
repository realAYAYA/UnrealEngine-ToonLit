// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace uba
{
	#if PLATFORM_WINDOWS
	using tchar = wchar_t;
	#else
	using tchar = char;
	#endif

	const tchar* GetVersionString();
}