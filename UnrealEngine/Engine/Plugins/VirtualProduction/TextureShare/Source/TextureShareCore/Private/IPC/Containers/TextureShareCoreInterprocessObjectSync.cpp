// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IPC/Containers/TextureShareCoreInterprocessObjectSync.h"

/////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreInterprocessObjectSync
/////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreInterprocessObjectSync::Initialize(const FTextureShareCoreObjectDesc& InObjectDesc, const FTextureShareCoreSyncSettings& InSyncSettings)
{
	Release();

	UpdateSettings(InObjectDesc, InSyncSettings);
	LastAccessTime.Update();
}

void FTextureShareCoreInterprocessObjectSync::Release()
{
	SyncSettings.Release();
	SyncState.Release();

	LastAccessTime.Empty();

	bProcessStuck = false;
}

void FTextureShareCoreInterprocessObjectSync::UpdateSettings(const FTextureShareCoreObjectDesc& InObjectDesc, const FTextureShareCoreSyncSettings& InSyncSettings)
{
	// Update sync settings
	SyncSettings.Initialize(InObjectDesc, InSyncSettings);
	UpdateLastAccessTime();
}

bool FTextureShareCoreInterprocessObjectSync::GetDesc(FTextureShareCoreObjectDesc& OutDesc) const
{
	if (IsEnabled())
	{
		// Return access time in cycle64
		OutDesc.Sync.LastAccessTime = LastAccessTime.Time;

		// Sync logic rules
		OutDesc.Sync.SyncStepSettings = SyncSettings.GetData();

		// and current sync state
		SyncState.Read(OutDesc.Sync.SyncState);

		return true;
	}

	return false;
}
