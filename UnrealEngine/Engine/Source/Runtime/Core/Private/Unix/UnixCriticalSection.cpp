// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unix/UnixCriticalSection.h"

#include "Containers/StringConv.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"

#include <fcntl.h>
#include <float.h>
#include <sys/file.h>
#include <unistd.h>

FUnixSystemWideCriticalSection::FUnixSystemWideCriticalSection(const FString& InName, FTimespan InTimeout)
{
	check(InName.Len() > 0)
	check(InTimeout >= FTimespan::Zero())
	check(InTimeout.GetTotalSeconds() < (double)FLT_MAX)

	const FString LockPath = FString(FPlatformProcess::ApplicationSettingsDir()) / InName;
	FString NormalizedFilepath(LockPath);
	NormalizedFilepath.ReplaceInline(TEXT("\\"), TEXT("/"));
	FileHandle = -1;

	double ExpireTimeSecs = FPlatformTime::Seconds() + InTimeout.GetTotalSeconds();
	while (true)
	{
		if (FileHandle == -1)
		{
			// Try to open the file.
			FileHandle = open(TCHAR_TO_UTF8(*NormalizedFilepath), O_CREAT | O_WRONLY | O_NONBLOCK, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		}

		// If the file is open, try to lock it, but don't block. If the file is already locked by another process or another thread, blocking here may not honor InTimeout.
		// NOTE: In old Linux kernels, flock() wasn't always atomic, but in recent ones, that was fixed and flock() is expected to be an atomic operation.
		if (FileHandle != -1 && flock(FileHandle, LOCK_EX | LOCK_NB) == 0)
		{
			return; // Lock was successfully taken.
		}

		// If the lock isn't acquired and no time is left to retry, clean up and set the state as 'invalid'
		if (InTimeout == FTimespan::Zero() || FPlatformTime::Seconds() > ExpireTimeSecs)
		{
			if (FileHandle != -1)
			{
				close(FileHandle);
				FileHandle = -1;
			}
			return; // Lock wasn't acquired within the allowed time.
		}

		// Either the file did not open or the lock wasn't acquired, retry.
		const float RetrySeconds = FMath::Min((float)InTimeout.GetTotalSeconds(), 0.25f);
		FPlatformProcess::Sleep(RetrySeconds);
	}
}

FUnixSystemWideCriticalSection::~FUnixSystemWideCriticalSection()
{
	Release();
}

bool FUnixSystemWideCriticalSection::IsValid() const
{
	return FileHandle != -1;
}

void FUnixSystemWideCriticalSection::Release()
{
	if (IsValid())
	{
		flock(FileHandle, LOCK_UN);
		close(FileHandle);
		FileHandle = -1;
	}
}
