// Copyright Epic Games, Inc. All Rights Reserved.

#include "Object/TextureShareCoreObject.h"
#include "Object/TextureShareCoreObjectContainers.h"

#include "IPC/TextureShareCoreInterprocessMemoryRegion.h"
#include "IPC/TextureShareCoreInterprocessMutex.h"
#include "IPC/TextureShareCoreInterprocessEvent.h"
#include "IPC/Containers/TextureShareCoreInterprocessMemory.h"

#include "Core/TextureShareCoreHelpers.h"

#include "Module/TextureShareCoreModule.h"
#include "Module/TextureShareCoreLog.h"

#include "Misc/ScopeLock.h"

//////////////////////////////////////////////////////////////////////////////////////////////
using namespace TextureShareCoreHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreObject
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::SetProcessId(const FString& InProcessId)
{
	if (InProcessId.IsEmpty() || ObjectDesc.ProcessDesc.ProcessId == InProcessId)
	{
		// Skip equal or empty values
		return false;
	}

	if (!IsSessionActive())
	{
		// Support local process name changes on the fly
		ObjectDesc.ProcessDesc.ProcessId = InProcessId;
	}
	else
	{
		// Deferred update
		NextProcessId = InProcessId;
	}

	return true;
}

bool FTextureShareCoreObject::SetDeviceType(const ETextureShareDeviceType InDeviceType)
{
	check(IsInGameThread());

	ObjectDesc.ProcessDesc.DeviceType = InDeviceType;

	UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:Set device type to '%s'"), *GetName(), GetTEXT(InDeviceType));

	return true;
}

void FTextureShareCoreObject::UpdateProcessId(FTextureShareCoreInterprocessObject& InOutObject)
{
	if (!NextProcessId.IsEmpty())
	{
		// Support local process name changes on the fly
		ObjectDesc.ProcessDesc.ProcessId = NextProcessId;

		// Update process name in shared memory at frame begin
		InOutObject.Desc.ProcessName.Initialize(NextProcessId);

		NextProcessId.Empty();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::SetSyncSetting(const FTextureShareCoreSyncSettings& InSyncSetting)
{
	if (!IsSessionActive())
	{
		SyncSettings = InSyncSetting;
	}
	else
	{
		if (!NewSyncSetting.IsValid())
		{
			NewSyncSetting = MakeShared<FTextureShareCoreSyncSettings, ESPMode::ThreadSafe>(InSyncSetting);
		}
		else
		{
			*NewSyncSetting = InSyncSetting;
		}
	}

	return true;
}

void FTextureShareCoreObject::UpdateFrameSyncSetting()
{
	if (NewSyncSetting.IsValid())
	{
		SyncSettings = *NewSyncSetting;
		NewSyncSetting.Reset();

		// Also update settings exposition for other processes
		if (IsActive() && Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
		{
			if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
			{
				if (FTextureShareCoreInterprocessObject* LocalObject = InterprocessMemory->FindObject(GetObjectDesc()))
				{
					LocalObject->UpdateSettings(GetObjectDesc(), SyncSettings);
				}
			}

			Owner.UnlockInterprocessMemory();
		}

		UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:UpdateFrameSyncSetting()"), *GetName());
	}
}

const FTextureShareCoreSyncSettings& FTextureShareCoreObject::GetSyncSetting() const
{
	return SyncSettings;
}

FTextureShareCoreFrameSyncSettings FTextureShareCoreObject::GetFrameSyncSettings(const ETextureShareFrameSyncTemplate InType) const
{
	FTextureShareCoreFrameSyncSettings Result;
	{
		switch (InType)
		{
		case ETextureShareFrameSyncTemplate::Default:
			Result.Steps.Append({
				// GameThread logic
				ETextureShareSyncStep::FrameBegin,
					ETextureShareSyncStep::FramePreSetupBegin,
				ETextureShareSyncStep::FrameEnd,

				// Proxy object sync settings (RenderingThread)
				ETextureShareSyncStep::FrameProxyBegin,

					ETextureShareSyncStep::FrameSceneFinalColorBegin,
					ETextureShareSyncStep::FrameSceneFinalColorEnd,

					ETextureShareSyncStep::FrameProxyPreRenderBegin,
					ETextureShareSyncStep::FrameProxyPreRenderEnd,

					ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentBegin,
					ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentEnd,

					ETextureShareSyncStep::FrameProxyFlush,
				ETextureShareSyncStep::FrameProxyEnd
				});
			break;

		case ETextureShareFrameSyncTemplate::SDK:
			Result.Steps.Append({
				// GameThread logic
				ETextureShareSyncStep::FrameBegin,
					ETextureShareSyncStep::FramePreSetupBegin,
				ETextureShareSyncStep::FrameEnd,

				// Proxy object sync settings (RenderingThread)
				ETextureShareSyncStep::FrameProxyBegin,
					ETextureShareSyncStep::FrameProxyPreRenderBegin,
					ETextureShareSyncStep::FrameProxyFlush,
				ETextureShareSyncStep::FrameProxyEnd
				});
			break;

		case ETextureShareFrameSyncTemplate::DisplayCluster:
			Result.Steps.Append({
				// GameThread logic
				ETextureShareSyncStep::FrameBegin,
					ETextureShareSyncStep::FramePreSetupBegin,
					ETextureShareSyncStep::FrameSetupBegin,
				ETextureShareSyncStep::FrameEnd,

				// Proxy object sync settings (RenderingThread)
				ETextureShareSyncStep::FrameProxyBegin,
					ETextureShareSyncStep::FrameProxyPreRenderBegin,
					ETextureShareSyncStep::FrameProxyPreRenderEnd,

					ETextureShareSyncStep::FrameProxyRenderBegin,
					ETextureShareSyncStep::FrameProxyRenderEnd,

					ETextureShareSyncStep::FrameProxyPostWarpBegin,
					ETextureShareSyncStep::FrameProxyPostWarpEnd,

					ETextureShareSyncStep::FrameProxyPostRenderBegin,
					ETextureShareSyncStep::FrameProxyPostRenderEnd,

					ETextureShareSyncStep::FrameProxyFlush,
				ETextureShareSyncStep::FrameProxyEnd
				});
			break;

		default:
			UE_LOG(LogTextureShareCoreObject, Error, TEXT("GetFrameSyncSettings: Not implemented for type '%s'"), GetTEXT(InType));
			break;
		}
	}

	return Result;
}
