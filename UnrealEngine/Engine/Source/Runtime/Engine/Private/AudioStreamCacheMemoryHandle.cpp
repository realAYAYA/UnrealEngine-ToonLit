// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioStreamCacheMemoryHandle.h"
#include "ContentStreaming.h"

FAudioStreamCacheMemoryHandle::FAudioStreamCacheMemoryHandle(FName InFeatureName, uint64 InMemoryUseInBytes)
: FeatureName(InFeatureName)
, MemoryUseInBytes(InMemoryUseInBytes)
{
	if (MemoryUseInBytes != 0)
	{
		IStreamingManager::Get().GetAudioStreamingManager().AddMemoryCountedFeature(*this);
	}
}

FAudioStreamCacheMemoryHandle::~FAudioStreamCacheMemoryHandle()
{
	if (MemoryUseInBytes != 0)
	{
		IStreamingManager::Get().GetAudioStreamingManager().RemoveMemoryCountedFeature(*this);
	}
}

void FAudioStreamCacheMemoryHandle::ResetMemoryUseInBytes(uint64 InMemoryUseInBytes)
{
	if (MemoryUseInBytes != 0)
	{
		IStreamingManager::Get().GetAudioStreamingManager().RemoveMemoryCountedFeature(*this);
	}
		
	MemoryUseInBytes = InMemoryUseInBytes;

	if (MemoryUseInBytes != 0)
	{
		IStreamingManager::Get().GetAudioStreamingManager().AddMemoryCountedFeature(*this);
	}
}