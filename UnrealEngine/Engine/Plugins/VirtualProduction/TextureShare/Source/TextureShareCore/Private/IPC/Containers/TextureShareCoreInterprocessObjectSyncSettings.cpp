// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPC/Containers/TextureShareCoreInterprocessObjectSyncSettings.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreInterprocessObjectSyncSettings
//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreInterprocessObjectSyncSettings::Initialize(const FTextureShareCoreObjectDesc& InObjectDesc, const FTextureShareCoreSyncSettings& InSyncSettings)
{
	Release();

	// Initialize process name rules
	ProcessName.Initialize(InObjectDesc.ProcessDesc.ProcessId);
	AllowedProcessNames.Initialize(InSyncSettings.FrameConnectionSettings.AllowedProcessNames);
	BannedProcessNames.Initialize(InSyncSettings.FrameConnectionSettings.BannedProcessNames);

	// Initialize bit-storage for used steps
	for (const ETextureShareSyncStep& StepIt : InSyncSettings.FrameSyncSettings.Steps)
	{
		const int8 BitIndex = (int8)StepIt;
		if (BitIndex >= 0 && BitIndex < 64)
		{
			Data |= (1ULL << BitIndex);
		}
	}

	// Update dead timeout value for local process
	ProcessLostStatusTimeOut = InSyncSettings.TimeoutSettings.ProcessLostStatusTimeOut;
}

void FTextureShareCoreInterprocessObjectSyncSettings::Release()
{
	FPlatformMemory::Memset(&Data, 0, sizeof(Data));

	ProcessLostStatusTimeOut = 0;
}
