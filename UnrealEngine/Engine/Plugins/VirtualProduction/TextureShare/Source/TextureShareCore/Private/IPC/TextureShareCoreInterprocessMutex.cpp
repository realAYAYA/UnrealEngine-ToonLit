// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPC/TextureShareCoreInterprocessMutex.h"
#include "Windows/WindowsPlatformProcess.h"
#include "Logging/LogScopedVerbosityOverride.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreInterprocessMutex
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareCoreInterprocessMutex::~FTextureShareCoreInterprocessMutex()
{
	ReleaseInterprocessMutex();
}

void FTextureShareCoreInterprocessMutex::ReleaseInterprocessMutex()
{
	if (PlatformMutex)
	{
		TryUnlockMutex();

		FPlatformProcess::FSemaphore* ProcessMutex = static_cast<FPlatformProcess::FSemaphore*>(PlatformMutex);
		PlatformMutex = nullptr;

		if (ProcessMutex)
		{
			LOG_SCOPE_VERBOSITY_OVERRIDE(LogHAL, ELogVerbosity::Error);
			FWindowsPlatformProcess::DeleteInterprocessSynchObject(ProcessMutex);
		}
	}
}

bool FTextureShareCoreInterprocessMutex::InitializeInterprocessMutex(bool bInGlobalNameSpace)
{
	if (PlatformMutex == nullptr)
	{
		const bool bCreateUniqueMutex = MutexId.IsEmpty();

		const FString MutexName = bCreateUniqueMutex ? FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensInBraces) : MutexId;
		const FString FullMutexName = (bInGlobalNameSpace && !bCreateUniqueMutex) ? FString::Printf(TEXT("Global\\%s"), *MutexName) : FString::Printf(TEXT("Local\\%s"), *MutexName);

		FPlatformProcess::FSemaphore* ProcessMutex = nullptr;

		if (!bCreateUniqueMutex)
		{
			LOG_SCOPE_VERBOSITY_OVERRIDE(LogHAL, ELogVerbosity::Error);

			// Open exist  mutex:
			ProcessMutex = FPlatformProcess::NewInterprocessSynchObject(*FullMutexName, false);
			if (ProcessMutex)
			{
				PlatformMutex = ProcessMutex;
				return true;
			}
		}

		// Create new:
		ProcessMutex = FPlatformProcess::NewInterprocessSynchObject(*FullMutexName, true);
		if (ProcessMutex)
		{
			PlatformMutex = ProcessMutex;
			return true;
		}
	}

	return false;
}

bool FTextureShareCoreInterprocessMutex::LockMutex(const uint32 InMaxMillisecondsToWait)
{
	if (PlatformMutex)
	{
		// 1ms = 10^6 ns
		const uint64 MaxNanosecondsToWait = InMaxMillisecondsToWait * 1000000ULL;

		FPlatformProcess::FSemaphore* ProcessMutex = static_cast<FPlatformProcess::FSemaphore*>(PlatformMutex);
		if (ProcessMutex)
		{
			if (!MaxNanosecondsToWait)
			{
				ProcessMutex->Lock();
			}
			else
			{
				if (!ProcessMutex->TryLock(MaxNanosecondsToWait))
				{
					return false;
				}
			}

			bLocked = true;

			return true;
		}
	}

	return false;
}

void FTextureShareCoreInterprocessMutex::UnlockMutex()
{
	if (PlatformMutex)
	{
		FPlatformProcess::FSemaphore* ProcessMutex = static_cast<FPlatformProcess::FSemaphore*>(PlatformMutex);
		if (ProcessMutex)
		{
			// relinquish
			bLocked = false;

			LOG_SCOPE_VERBOSITY_OVERRIDE(LogHAL, ELogVerbosity::Error);
			ProcessMutex->Unlock();
		}
	}
}

void FTextureShareCoreInterprocessMutex::TryUnlockMutex()
{
	if (bLocked)
	{
		UnlockMutex();
	}
}
