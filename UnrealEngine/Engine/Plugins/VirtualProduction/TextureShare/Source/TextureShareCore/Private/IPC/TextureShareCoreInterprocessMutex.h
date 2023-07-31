// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IPC mutex
 */
class FTextureShareCoreInterprocessMutex
{
public:
	FTextureShareCoreInterprocessMutex()
		: MutexId()
	{ }

	FTextureShareCoreInterprocessMutex(const FString& InMutexId)
		: MutexId(InMutexId)
	{ }


	~FTextureShareCoreInterprocessMutex();

	bool Initialize()
	{
		return InitializeInterprocessMutex(true);
	}

	bool LockMutex(const uint32 InMaxMillisecondsToWait);
	void UnlockMutex();

	void TryUnlockMutex();

private:
	bool InitializeInterprocessMutex(bool bGlobalNameSpace);
	void ReleaseInterprocessMutex();

public:
	const FString MutexId;

private:
	void* PlatformMutex = nullptr;
	bool bLocked = false;
};
