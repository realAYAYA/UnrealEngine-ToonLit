// Copyright Epic Games, Inc. All Rights Reserved.

#include "SharedMemoryMediaSource.h"

#include "MediaIOCorePlayerBase.h"
#include "SharedMemoryMediaSourceOptions.h"


bool USharedMemoryMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == SharedMemoryMediaOption::ZeroLatency)
	{
		return bZeroLatency;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

int64 USharedMemoryMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	if (Key == SharedMemoryMediaOption::Mode)
	{
		return int64(Mode);
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

FString USharedMemoryMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	if (Key == SharedMemoryMediaOption::UniqueName)
	{
		return UniqueName;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

bool USharedMemoryMediaSource::HasMediaOption(const FName& Key) const
{
	if (Key == SharedMemoryMediaOption::UniqueName)
	{
		return true;
	}

	if (Key == SharedMemoryMediaOption::ZeroLatency)
	{
		return true;
	}

	if (Key == SharedMemoryMediaOption::Mode)
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}

FString USharedMemoryMediaSource::GetUrl() const
{
	return FString::Printf(TEXT("dcsm://%s"), *UniqueName);
}

bool USharedMemoryMediaSource::Validate() const
{
	return true;
}
