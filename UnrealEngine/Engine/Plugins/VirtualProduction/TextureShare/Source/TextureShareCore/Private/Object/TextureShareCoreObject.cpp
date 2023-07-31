// Copyright Epic Games, Inc. All Rights Reserved.

#include "Object/TextureShareCoreObject.h"
#include "Object/TextureShareCoreObjectContainers.h"

#include "IPC/TextureShareCoreInterprocessMemoryRegion.h"
#include "IPC/TextureShareCoreInterprocessMutex.h"
#include "IPC/TextureShareCoreInterprocessEvent.h"
#include "IPC/Containers/TextureShareCoreInterprocessMemory.h"

#include "Core/TextureShareCore.h"
#include "Core/TextureShareCoreHelpers.h"
#include "Module/TextureShareCoreLog.h"

#include "Misc/ScopeLock.h"

//////////////////////////////////////////////////////////////////////////////////////////////
using namespace TextureShareCoreHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareCoreObjectHelpers
{
	static FTextureShareCoreObjectDesc CreateObjectObjectDesc(const FTextureShareCore& InOwner, const FString& InTextureShareName)
	{
		FTextureShareCoreObjectDesc ObjectDesc;

		// The name of this object
		ObjectDesc.ShareName = InTextureShareName;

		// Generate unique Guid for each object
		ObjectDesc.ObjectGuid = FGuid::NewGuid();

		// Get value for Owner
		ObjectDesc.ProcessDesc = InOwner.GetProcessDesc();

		return ObjectDesc;
	}
};
using namespace TextureShareCoreObjectHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreObject
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareCoreObject::FTextureShareCoreObject(FTextureShareCore& InOwner, const FString& InTextureShareName)
	: ObjectDesc(CreateObjectObjectDesc(InOwner, InTextureShareName))
	, Owner(InOwner)
{
	NotificationEvent = Owner.CreateInterprocessEvent(ObjectDesc.ObjectGuid);
}

FTextureShareCoreObject::~FTextureShareCoreObject()
{
	EndSession();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreObject::HandleResetSync(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject)
{
#if TEXTURESHARECORE_DEBUGLOG
	InterprocessMemory.UpdateFrameConnections(FrameConnections);
	UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:ResetSync(%s) %s"), *GetName(), *ToString(LocalObject), *ToString(FrameConnections));
#endif

	// Unlock thread mutexes
	ResetThreadMutexes();

	SetCurrentSyncStep(ETextureShareSyncStep::Undefined);

	LocalObject.Sync.ResetSync();

	SetFrameSyncState(ETextureShareCoreInterprocessObjectFrameSyncState::FrameSyncLost);

	// Reset current frame connections
	FrameConnections.Reset();
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreObject::SendNotificationEvents(const bool bInterprocessMemoryLockRequired)
{
	if (IsSessionActive() && IsActive())
	{
		if (!bInterprocessMemoryLockRequired || Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
		{
			if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
			{
				// Wake up all connectable processes
				TArray<const FTextureShareCoreInterprocessObject*> EventListeners;
				if (InterprocessMemory->FindObjectEventListeners(EventListeners, GetObjectDesc()))
				{
					for (const FTextureShareCoreInterprocessObject* RemoteObjectIt : EventListeners)
					{
						const FGuid EventGuid = RemoteObjectIt->Desc.ObjectGuid.ToGuid();

						if (!CachedNotificationEvents.Contains(EventGuid))
						{
							TSharedPtr<FEvent, ESPMode::ThreadSafe> Event = Owner.OpenInterprocessEvent(EventGuid);
							if (!Event.IsValid())
							{
								UE_LOG(LogTextureShareCoreObject, Error, TEXT("TS'%s': Can't open remote event from process '%s'"), *GetName(), *RemoteObjectIt->Desc.ProcessName.ToString());
							}

							CachedNotificationEvents.Emplace(EventGuid, Event);
						}

						if (CachedNotificationEvents[EventGuid].IsValid())
						{
							// Send wake up signal
							CachedNotificationEvents[EventGuid]->Trigger();
						}
					}
				}
			}

			if (bInterprocessMemoryLockRequired)
			{
				Owner.UnlockInterprocessMemory();
			}
		}
	}
}

void FTextureShareCoreObject::HandleFrameSkip(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject)
{
	UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:FrameSkip()"), *GetName());

	// Wake up remote processes anywait, because we change mem object header
	SendNotificationEvents(false);

	// Reset frame sync fro this frame
	HandleResetSync(InterprocessMemory, LocalObject);
}

void FTextureShareCoreObject::HandleFrameLost(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject)
{
	// Local frame connection lost
	UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:FrameLost()"), *GetName());

	// Wake up remote processes anywait, because we change mem object header
	SendNotificationEvents(false);

	// Reset frame sync fro this frame
	HandleResetSync(InterprocessMemory, LocalObject);
}

bool FTextureShareCoreObject::TryWaitFrameProcesses(const uint32 InRemainMaxMillisecondsToWait)
{
	// Wake up remote processes anywait, because we change mem object header
	SendNotificationEvents(false);

	// Reset notification event
	NotificationEvent->Reset();

	// Unlock IPC shared memory
	Owner.UnlockInterprocessMemory();

	// Wait for remote processes data changed
	NotificationEvent->Wait(InRemainMaxMillisecondsToWait);

	// Try lock shared memoru back
	return Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout);
}

bool FTextureShareCoreObject::RemoveObject()
{
	return Owner.RemoveCoreObject(GetName());
}
