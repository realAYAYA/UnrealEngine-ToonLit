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

#include "ITextureShareCoreCallbacks.h"

//////////////////////////////////////////////////////////////////////////////////////////////
using namespace TextureShareCoreHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareCoreObject
//////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::FrameSync(const ETextureShareSyncStep InSyncStep)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TextureShareCore::FrameSync);

	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT(">>>> %s:FrameSync(%s)"), *GetName(), GetTEXT(InSyncStep));

	if (IsFrameSyncActive() && TryEnterSyncBarrier(InSyncStep))
	{
		int32 Index = SyncSettings.FrameSyncSettings.Steps.Find(CurrentSyncStep);
		UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:FrameSync(%s) Index=%d"), *GetName(), GetTEXT(InSyncStep), Index);

		if (Index != INDEX_NONE)
		{
			// sync all skipped steps
			for (; Index < SyncSettings.FrameSyncSettings.Steps.Num(); Index++)
			{
				const ETextureShareSyncStep SyncStepIt = SyncSettings.FrameSyncSettings.Steps[Index];
				if (SyncStepIt != CurrentSyncStep)
				{
					if (SyncStepIt >= ETextureShareSyncStep::FrameEnd)
					{
						return false;
					}

					if (SyncStepIt == InSyncStep)
					{
						break;
					}

					// Force to call skipped sync steps
					UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:ForceFrameSync(%s)"), *GetName(), GetTEXT(SyncStepIt));
					if (!DoFrameSync(SyncStepIt))
					{
						return false;
					}
				}
			}
		}

		return DoFrameSync(InSyncStep);
	}

	return false;
};

bool FTextureShareCoreObject::FrameSync_RenderThread(const ETextureShareSyncStep InSyncStep)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TextureShareCore::FrameSync_RenderThread);

	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT(">>>> %s:FrameSync_RenderThread(%s)"), *GetName(), GetTEXT(InSyncStep));

	if (IsFrameSyncActive() && TryEnterSyncBarrier(InSyncStep))
	{
		int32 Index = SyncSettings.FrameSyncSettings.Steps.Find(CurrentSyncStep);
		UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:FrameSync_RenderThread(%s) Index=%d"), *GetName(), GetTEXT(InSyncStep), Index);

		if (Index != INDEX_NONE)
		{
			// sync all skipped steps
			for (Index; Index < SyncSettings.FrameSyncSettings.Steps.Num(); Index++)
			{
				const ETextureShareSyncStep SyncStepIt = SyncSettings.FrameSyncSettings.Steps[Index];

				UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:check sync step (%s) Index=%d"), *GetName(), GetTEXT(SyncStepIt), Index);

				if (SyncStepIt != CurrentSyncStep)
				{
					if (SyncStepIt >= ETextureShareSyncStep::FrameProxyEnd)
					{
						return false;
					}

					if (SyncStepIt == InSyncStep)
					{
						break;
					}

					// Force to call skipped sync steps
					UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:ForceFrameSync_RenderThread(%s)"), *GetName(), GetTEXT(SyncStepIt));
					if (!DoFrameSync_RenderThread(SyncStepIt))
					{
						return false;
					}
				}
			}
		}

		return DoFrameSync_RenderThread(InSyncStep);
	}

	return false;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::IsBeginFrameSyncActive() const
{
	switch (FrameSyncState)
	{
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyBegin:
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyEnd:
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameEnd:
	case ETextureShareCoreInterprocessObjectFrameSyncState::Undefined:
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameSyncLost:
		break;
	default:
		// logic broken
		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:BeginFrameSync() - frame logic broken = %s"), *GetName(), GetTEXT(FrameSyncState));
		return false;
	}

	return true;
}

bool FTextureShareCoreObject::BeginFrameSync()
{
	if (!IsBeginFrameSyncActive())
	{
		return false;
	}

	// Reset prev-frame data
	Data.ResetData();

	// Update sync frame settings before new frame begin
	UpdateFrameSyncSetting();

	// And connect new frame processes (updates every frame)
	if (ConnectFrameProcesses())
	{
		SetCurrentSyncStep(ETextureShareSyncStep::FrameBegin);
		SetFrameSyncState(ETextureShareCoreInterprocessObjectFrameSyncState::FrameBegin);

		if (ITextureShareCoreCallbacks::Get().OnTextureShareCoreBeginFrameSync().IsBound())
		{
			ITextureShareCoreCallbacks::Get().OnTextureShareCoreBeginFrameSync().Broadcast(*this);
		}

		return true;
	}

	return false;
}

bool FTextureShareCoreObject::EndFrameSync()
{
	if (IsFrameSyncActive() && FrameSyncState == ETextureShareCoreInterprocessObjectFrameSyncState::FrameBegin)
	{
		SetCurrentSyncStep(ETextureShareSyncStep::FrameEnd);
		SetFrameSyncState(ETextureShareCoreInterprocessObjectFrameSyncState::FrameEnd);


		if (ITextureShareCoreCallbacks::Get().OnTextureShareCoreEndFrameSync().IsBound())
		{
			ITextureShareCoreCallbacks::Get().OnTextureShareCoreEndFrameSync().Broadcast(*this);
		}

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::IsBeginFrameSyncActive_RenderThread() const
{
	switch (FrameSyncState)
	{
	default:
		break;
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyBegin:
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyEnd:
		// logic broken
		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:BeginFrameProxySync() - frame logic broken = %s"), *GetName(), GetTEXT(FrameSyncState));
		return false;
	}

	return true;
}

bool FTextureShareCoreObject::BeginFrameSync_RenderThread()
{
	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:BeginFrameSync_RenderThread() < in"), *GetName());

	if (IsBeginFrameSyncActive_RenderThread())
	{
		// Reset prev-frame proxy data
		ProxyData.ResetProxyData();

		SetCurrentSyncStep(ETextureShareSyncStep::FrameProxyBegin);
		SetFrameSyncState(ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyBegin);

		if (ITextureShareCoreCallbacks::Get().OnTextureShareCoreBeginFrameSync_RenderThread().IsBound())
		{
			ITextureShareCoreCallbacks::Get().OnTextureShareCoreBeginFrameSync_RenderThread().Broadcast(*this);
		}

		return true;
	}

	return false;
}

bool FTextureShareCoreObject::EndFrameSync_RenderThread()
{
	bool bResult = false;

	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:EndFrameSync_RenderThread() < in"), *GetName());

	// Always force flush sync for render proxy
	FrameSync_RenderThread(ETextureShareSyncStep::FrameProxyFlush);

	if (IsFrameSyncActive() && FrameSyncState == ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyBegin)
	{
		// And finally disconnect frame processes
		bResult = DisconnectFrameProcesses();

		SetCurrentSyncStep(ETextureShareSyncStep::FrameProxyEnd);
		SetFrameSyncState(ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyEnd);

		if (ITextureShareCoreCallbacks::Get().OnTextureShareCoreEndFrameSync_RenderThread().IsBound())
		{
			ITextureShareCoreCallbacks::Get().OnTextureShareCoreEndFrameSync_RenderThread().Broadcast(*this);
		}
	}

	return bResult;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreObject::SetCurrentSyncStep(const ETextureShareSyncStep InCurrentSyncStep)
{
	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:SetCurrentSyncStep(%s->%s)"), *GetName(), GetTEXT(CurrentSyncStep), GetTEXT(InCurrentSyncStep));

	CurrentSyncStep = InCurrentSyncStep;
}

void FTextureShareCoreObject::SetFrameSyncState(const ETextureShareCoreInterprocessObjectFrameSyncState InFrameSyncState)
{
	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:SetFrameSyncState(%s->%s)"), *GetName(), GetTEXT(FrameSyncState), GetTEXT(InFrameSyncState));

	FrameSyncState = InFrameSyncState;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareCoreObject::UpdateLastAccessTime() const
{
	if (IsSessionActive() && IsActive() && Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
	{
		if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
		{
			// Get existing IPC object memory region
			if (FTextureShareCoreInterprocessObject* InterprocessObject = InterprocessMemory->FindObject(GetObjectDesc()))
			{
				InterprocessObject->Sync.UpdateLastAccessTime();
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool FTextureShareCoreObject::DoFrameSync(const ETextureShareSyncStep InSyncStep)
{
	if (IsFrameSyncActive() && TryEnterSyncBarrier(InSyncStep))
	{
		if (!PrepareSyncBarrierPass(InSyncStep))
		{
			UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:FrameSync(%s) - Skipped"), *GetName(), GetTEXT(InSyncStep));
			SetCurrentSyncStep(InSyncStep);

			// Skip this sync step - other processes not support
			return true;
		}

		UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:FrameSync(%s)"), *GetName(), GetTEXT(InSyncStep));

		// Write local data
		SendFrameData();

		if (ITextureShareCoreCallbacks::Get().OnTextureShareCoreFrameSync().IsBound())
		{
			ITextureShareCoreCallbacks::Get().OnTextureShareCoreFrameSync().Broadcast(*this, InSyncStep);
		}

		// add barrier here
		if (SyncBarrierPass(InSyncStep, ETextureShareSyncPass::Enter))
		{
			SetCurrentSyncStep(InSyncStep);
			ReceiveFrameData();

			return SyncBarrierPass(InSyncStep, ETextureShareSyncPass::Exit);
		}

		// ?? barrier failed/timeout etc
		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:FrameSync(%s) - failed"), *GetName(), GetTEXT(InSyncStep));

		return false;
	}

	return false;
}

bool FTextureShareCoreObject::DoFrameSync_RenderThread(const ETextureShareSyncStep InSyncStep)
{
	if (IsFrameSyncActive() && TryEnterSyncBarrier(InSyncStep))
	{
		if (!PrepareSyncBarrierPass(InSyncStep))
		{
			UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:FrameSync_RenderThread(%s) - Skipped"), *GetName(), GetTEXT(InSyncStep));
			SetCurrentSyncStep(InSyncStep);

			// Skip this sync step - other processes not support
			return true;
		}

		UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:FrameSync_RenderThread(%s)"), *GetName(), GetTEXT(InSyncStep));

		// Write local data
		SendFrameProxyData_RenderThread();

		if (ITextureShareCoreCallbacks::Get().OnTextureShareCoreFrameSync_RenderThread().IsBound())
		{
			ITextureShareCoreCallbacks::Get().OnTextureShareCoreFrameSync_RenderThread().Broadcast(*this, InSyncStep);
		}

		// add barrier here
		if (SyncBarrierPass(InSyncStep, ETextureShareSyncPass::Enter))
		{
			SetCurrentSyncStep(InSyncStep);
			ReceiveFrameProxyData_RenderThread();

			return SyncBarrierPass(InSyncStep, ETextureShareSyncPass::Exit);
		}

		// ?? barrier failed/timeout etc
		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:FrameSync_RenderThread(%s) - failed"), *GetName(), GetTEXT(InSyncStep));

		return false;
	}

	return false;
}

void FTextureShareCoreObject::ReleaseSyncData()
{
	UE_TS_LOG(LogTextureShareCoreObject, Log, TEXT("%s:ReleaseSyncData()"), *GetName());

	FrameConnections.Empty();

	FrameSyncState = ETextureShareCoreInterprocessObjectFrameSyncState::Undefined;
	CurrentSyncStep = ETextureShareSyncStep::Undefined;

	CachedNotificationEvents.Empty();
}
