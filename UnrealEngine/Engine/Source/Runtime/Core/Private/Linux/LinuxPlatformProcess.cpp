// Copyright Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxPlatformProcess.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"

const TCHAR* FLinuxPlatformProcess::BaseDir()
{
	static bool bHaveResult = false;
	static TCHAR CachedResult[UNIX_MAX_PATH];
	if (!bHaveResult)
	{
		char SelfPath[UNIX_MAX_PATH] = {0};
		if (readlink( "/proc/self/exe", SelfPath, UE_ARRAY_COUNT(SelfPath) - 1) == -1)
		{
			int ErrNo = errno;
			UE_LOG(LogHAL, Fatal, TEXT("readlink() failed with errno = %d (%s)"), ErrNo,
				StringCast< TCHAR >(strerror(ErrNo)).Get());
			// unreachable
			return CachedResult;
		}
		SelfPath[UE_ARRAY_COUNT(SelfPath) - 1] = 0;

		FCString::Strncpy(CachedResult, UTF8_TO_TCHAR(dirname(SelfPath)), UE_ARRAY_COUNT(CachedResult) - 1);
		CachedResult[UE_ARRAY_COUNT(CachedResult) - 1] = 0;
		FCString::Strncat(CachedResult, TEXT("/"), UE_ARRAY_COUNT(CachedResult) - 1);

#ifdef UE_RELATIVE_BASE_DIR
		FString CollapseResult(CachedResult);
		CollapseResult /= UE_RELATIVE_BASE_DIR;
		FPaths::CollapseRelativeDirectories(CollapseResult);
		FCString::Strcpy(CachedResult, UNIX_MAX_PATH, *CollapseResult);
#endif

		bHaveResult = true;
	}
	return CachedResult;
}

const TCHAR* FLinuxPlatformProcess::GetBinariesSubdirectory()
{
	if (PLATFORM_CPU_ARM_FAMILY)
	{
		return TEXT("LinuxArm64");
	}

	return TEXT("Linux");
}
