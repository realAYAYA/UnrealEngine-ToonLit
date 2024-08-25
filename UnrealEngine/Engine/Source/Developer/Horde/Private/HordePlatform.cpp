// Copyright Epic Games, Inc. All Rights Reserved.

#include "HordePlatform.h"
#include <assert.h>
#include <wchar.h>
#include <bit>
#include <algorithm>
#include <iostream>

// Defines for the current platform
#ifdef _MSC_VER
#define UE_HORDE_PLATFORM_WINDOWS 1
#define UE_HORDE_PLATFORM_MAC 0
#define UE_HORDE_PLATFORM_LINUX 0
#elif defined(__APPLE__)
#define UE_HORDE_PLATFORM_WINDOWS 0
#define UE_HORDE_PLATFORM_MAC 1
#define UE_HORDE_PLATFORM_LINUX 0
#else
#define UE_HORDE_PLATFORM_WINDOWS 0
#define UE_HORDE_PLATFORM_MAC 0
#define UE_HORDE_PLATFORM_LINUX 1
#endif

#if UE_HORDE_PLATFORM_WINDOWS
#include <Windows.h>
#undef min
#undef max
#undef GetEnvironmentVariable
#undef SendMessage
#undef InterlockedIncrement
#else
#include <semaphore.h>
#include <unistd.h>
#include <atomic>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#endif

void FHordePlatform::NotImplemented()
{
	throw std::string("Not Implemented");
}

void FHordePlatform::NotSupported(const char* Message)
{
	throw std::string(Message);
}

bool FHordePlatform::GetEnvironmentVariable(const char* Name, char* Buffer, size_t BufferLen)
{
#if PLATFORM_WINDOWS
	int Length = GetEnvironmentVariableA(Name, Buffer, (DWORD)BufferLen);
	return Length > 0 && Length < BufferLen;
#else
	char* Value = getenv(Name);
	if (Value != nullptr)
	{
		FCStringAnsi::Strcpy(Buffer, BufferLen, Value);
		return true;
	}
	return false;
#endif
}

void FHordePlatform::CreateUniqueIdentifier(char* NameBuffer, size_t NameBufferLen)
{
	static int32 Counter = 0;

#if UE_HORDE_PLATFORM_WINDOWS
	DWORD Pid = GetCurrentProcessId();
	ULONGLONG TickCount = GetTickCount64();
	snprintf(NameBuffer, NameBufferLen, "%lu_%llu_%lu", Pid, TickCount, (unsigned long)FPlatformAtomics::InterlockedIncrement(&Counter));
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	snprintf(NameBuffer, NameBufferLen, "%u%zu%zu_%d", getpid(), (size_t)ts.tv_sec, (size_t)ts.tv_nsec, FPlatformAtomics::InterlockedIncrement(&Counter));
#endif
}

void FHordePlatform::CreateUniqueName(char* NameBuffer, size_t NameBufferLen)
{
#if UE_HORDE_PLATFORM_WINDOWS
	const char Prefix[] = "Local\\COMPUTE_";
#else
	const char Prefix[] = "/UEC_";
#endif

	size_t PrefixLen = (sizeof(Prefix) / sizeof(Prefix[0])) - 1;
	check(NameBufferLen > PrefixLen);

	memcpy(NameBuffer, Prefix, PrefixLen * sizeof(char));
	CreateUniqueIdentifier(NameBuffer + PrefixLen, NameBufferLen - PrefixLen);
}

unsigned int FHordePlatform::FloorLog2(unsigned int Value)
{
	return std::max<unsigned int>(0, 31 - std::countl_zero(Value));
}

unsigned int FHordePlatform::CountLeadingZeros(unsigned int Value)
{
	return std::countl_zero(Value);
}

bool FHordePlatform::TryParseSizeT(const char* Source, size_t SourceLen, size_t& OutValue, size_t& OutNumBytes)
{
	OutValue = 0;
	OutNumBytes = 0;

	for (; OutNumBytes < SourceLen; OutNumBytes++)
	{
		char Character = Source[OutNumBytes];
		if (Character < '0' || Character > '9')
		{
			break;
		}
		OutValue = (OutValue * 10) + (Character - '0');
	}

	return OutNumBytes > 0;
}
