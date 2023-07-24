// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if PLATFORM_LINUX

#include <codecvt>
#include <locale>
#include <thread>


// provide string the way Unreal Engine expects
#define TCHAR char16_t

// convert Windows style enums in Linux, i.e. BMDPixelFormat to _BMDPixelFormat
#define ENUM(x) _ ## x

// simulate Windows BOOL type
typedef bool BOOL;

class IDeckLink;
class IDeckLinkDisplayMode;
class IDeckLinkIterator;
class IDeckLinkVideoConversion;

namespace BlackmagicPlatform
{
	std::string GetName(IDeckLinkDisplayMode* DisplayMode);

	bool InitializeAPI();
	void ReleaseAPI();

	IDeckLinkIterator* CreateDeckLinkIterator();
	void DestroyDeckLinkIterator(IDeckLinkIterator*);

	IDeckLinkVideoConversion* CreateDeckLinkVideoConversion();
	void DestroyDeckLinkVideoConversion(IDeckLinkVideoConversion*);

	void SetThreadPriority_TimeCritical(std::thread& InThread);

	bool GetDisplayName(IDeckLink* Device, TCHAR* OutDisplayName, int32_t Size);

	void* Allocate(uint32_t Size);
	bool Free(void* Address, uint32_t Size);
}

#endif