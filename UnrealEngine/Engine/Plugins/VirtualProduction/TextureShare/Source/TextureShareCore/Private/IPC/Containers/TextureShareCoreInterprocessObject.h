// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IPC/Containers/TextureShareCoreInterprocessObjectDesc.h"
#include "IPC/Containers/TextureShareCoreInterprocessObjectData.h"
#include "IPC/Containers/TextureShareCoreInterprocessObjectSync.h"

/**
 * This structure represents the data of a TextureShare object in shared memory.
 * This structure fits the criteria of being POD ("Plain Old Data") for binary compatibility with direct memory access.
 */
struct FTextureShareCoreInterprocessObject
{
	// Object ID data
	FTextureShareCoreInterprocessObjectDesc Desc;

	// Object sync info
	FTextureShareCoreInterprocessObjectSync Sync;

	// Object data for IPC exchanges
	FTextureShareCoreInterprocessObjectData Data;

public:
	bool IsEnabled() const
	{
		return Desc.IsEnabled() && Sync.IsEnabled();
	}

	bool GetDesc(FTextureShareCoreObjectDesc& OutDesc) const
	{
		return Desc.GetDesc(OutDesc) && Sync.GetDesc(OutDesc);
	}

	void UpdateSettings(const FTextureShareCoreObjectDesc& InObjectDesc, const FTextureShareCoreSyncSettings& InSyncSettings)
	{
		Sync.UpdateSettings(InObjectDesc, InSyncSettings);
	}

	void Initialize(const FTextureShareCoreObjectDesc& InObjectDesc, const FTextureShareCoreSyncSettings& InSyncSettings)
	{
		Desc.Initialize(InObjectDesc);
		Sync.Initialize(InObjectDesc, InSyncSettings);
		Data.Initialize();
	}

	void Release()
	{
		Desc.Release();
		Sync.Release();
		Data.Release();
	}
};
