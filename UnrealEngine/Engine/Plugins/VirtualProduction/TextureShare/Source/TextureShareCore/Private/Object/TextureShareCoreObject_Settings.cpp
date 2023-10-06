// Copyright Epic Games, Inc. All Rights Reserved.

#include "Object/TextureShareCoreObject.h"
#include "Object/TextureShareCoreObjectContainers.h"

#include "IPC/TextureShareCoreInterprocessMemoryRegion.h"
#include "IPC/TextureShareCoreInterprocessMutex.h"
#include "IPC/TextureShareCoreInterprocessEvent.h"
#include "IPC/Containers/TextureShareCoreInterprocessMemory.h"
#include "IPC/TextureShareCoreInterprocessHelpers.h"

#include "Module/TextureShareCoreModule.h"
#include "Module/TextureShareCoreLog.h"

#include "Misc/ScopeLock.h"

using namespace UE::TextureShareCore;

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
		if (LockThreadMutex(ETextureShareThreadMutex::InternalLock))
		{
			ObjectDesc.ProcessDesc.ProcessId = InProcessId;

			// Update IPC
			UpdateInterprocessObject();

			UnlockThreadMutex(ETextureShareThreadMutex::InternalLock);
		}
	}

	UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:SetProcessId('%s')"), *GetName(), *InProcessId);

	return true;
}

bool FTextureShareCoreObject::SetDeviceType(const ETextureShareDeviceType InDeviceType)
{
	if (!IsSessionActive())
	{
		ObjectDesc.ProcessDesc.DeviceType = InDeviceType;
	}
	else
	{
		if (LockThreadMutex(ETextureShareThreadMutex::InternalLock))
		{
			ObjectDesc.ProcessDesc.DeviceType = InDeviceType;

			// Update IPC
			UpdateInterprocessObject();

			UnlockThreadMutex(ETextureShareThreadMutex::InternalLock);
		}
	}

	UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:SetDeviceType(%s)"), *GetName(), GetTEXT(InDeviceType));

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::SetSyncSetting(const FTextureShareCoreSyncSettings& InSyncSettings)
{
	if (!IsSessionActive())
	{
		SyncSettings = InSyncSettings;
	}
	else
	{
		if (SyncSettings ==InSyncSettings)
		{
			return true;
		}

		if (LockThreadMutex(ETextureShareThreadMutex::InternalLock))
		{
			// Update sync settings
			SyncSettings = InSyncSettings;

			// Update object desc sync hash value
			ObjectDesc.Sync.SetSyncStepSettings(SyncSettings);

			// Update IPC
			UpdateInterprocessObject();

			UnlockThreadMutex(ETextureShareThreadMutex::InternalLock);
		}
	}

	UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:SetSyncSetting()"), *GetName());

	return true;
}

void FTextureShareCoreObject::AddNewSyncStep(const ETextureShareSyncStep InSyncStep)
{
	if (!IsSessionActive())
	{
		SyncSettings.FrameSyncSettings.Steps.AddSorted(InSyncStep);
	}
	else
	{
		if (LockThreadMutex(ETextureShareThreadMutex::InternalLock))
		{
			// Add requested sync step
			SyncSettings.FrameSyncSettings.Steps.AddSorted(InSyncStep);

			// Update object desc sync hash value
			ObjectDesc.Sync.SetSyncStepSettings(SyncSettings);

			// Update IPC
			UpdateInterprocessObject();

			UnlockThreadMutex(ETextureShareThreadMutex::InternalLock);
		}
	}
}

void FTextureShareCoreObject::UpdateInterprocessObject()
{
	// Also update settings exposition for other processes
	if (IsActive() && Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
	{
		if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
		{
			if (FTextureShareCoreInterprocessObject* LocalObject = InterprocessMemory->FindObject(GetObjectDesc()))
			{
				LocalObject->UpdateInterprocessObject(GetObjectDesc(), SyncSettings);
			}
		}

		Owner.UnlockInterprocessMemory();
	}

	UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:UpdateInterprocessObject()"), *GetName());
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

					ETextureShareSyncStep::FrameFlush,
				ETextureShareSyncStep::FrameEnd,

				// Proxy object sync settings (RenderingThread)
				ETextureShareSyncStep::FrameProxyBegin,

					ETextureShareSyncStep::FrameSceneFinalColorEnd,
					ETextureShareSyncStep::FrameProxyPreRenderEnd,
					ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentEnd,

					ETextureShareSyncStep::FrameProxyFlush,
				ETextureShareSyncStep::FrameProxyEnd
				});
			break;

		case ETextureShareFrameSyncTemplate::SDK:
			Result.Steps.Append({
				// GameThread logic
				ETextureShareSyncStep::FrameBegin,

					// Synchronization steps are added upon request from the SDK

					ETextureShareSyncStep::FrameFlush,
				ETextureShareSyncStep::FrameEnd,

				// Proxy object sync settings (RenderingThread)
				ETextureShareSyncStep::FrameProxyBegin,

					// Synchronization steps are added upon request from the SDK

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

					ETextureShareSyncStep::FrameFlush,
				ETextureShareSyncStep::FrameEnd,

				// Proxy object sync settings (RenderingThread)
				ETextureShareSyncStep::FrameProxyBegin,

					ETextureShareSyncStep::FrameProxyPreRenderEnd,
					ETextureShareSyncStep::FrameProxyRenderEnd,
					ETextureShareSyncStep::FrameProxyPostWarpEnd,
					ETextureShareSyncStep::FrameProxyPostRenderEnd,

					ETextureShareSyncStep::FrameProxyFlush,
				ETextureShareSyncStep::FrameProxyEnd
				});
			break;

		case ETextureShareFrameSyncTemplate::DisplayClusterCrossNode:
			Result.Steps.Append({
				// GameThread logic
				ETextureShareSyncStep::FrameBegin,
					// Synchronization steps are added upon request
					ETextureShareSyncStep::FrameFlush,
				ETextureShareSyncStep::FrameEnd,

				// Proxy object sync settings (RenderingThread)
				ETextureShareSyncStep::FrameProxyBegin,
					// Synchronization steps are added upon request
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
