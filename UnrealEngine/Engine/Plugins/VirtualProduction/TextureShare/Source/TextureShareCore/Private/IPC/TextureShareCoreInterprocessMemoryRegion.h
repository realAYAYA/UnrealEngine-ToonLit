// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

/**
 * IPC Shared Memory
 */
class FTextureShareCoreInterprocessMemoryRegion
{
public:
	FTextureShareCoreInterprocessMemoryRegion(const FString& InSharedMemoryRegionName);
	~FTextureShareCoreInterprocessMemoryRegion();

	// Initialize shared memory resources
	bool Initialize();

	struct FTextureShareCoreInterprocessMemory* GetInterprocessMemory() const;

private:
	bool CleanupInterprocessObjects() const;
	void ReleaseInterprocessObjects();

public:
	const FString SharedMemoryRegionName;

private:
	void* PlatformMemoryRegion = nullptr;
};
