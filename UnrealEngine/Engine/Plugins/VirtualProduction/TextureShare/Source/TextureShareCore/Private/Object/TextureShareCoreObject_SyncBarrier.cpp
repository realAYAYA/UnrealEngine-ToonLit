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
bool FTextureShareCoreObject::BeginSyncBarrier(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject, const ETextureShareSyncStep InSyncStep, const ETextureShareSyncPass InSyncPass)
{
	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:BeginSyncBarrier(%s[%s]) %s In << %s >>"), *GetName(), GetTEXT(InSyncStep), GetTEXT(InSyncPass), *ToString(LocalObject), *ToString(FrameConnections));

	if (LocalObject.Sync.BeginSyncBarrier(InSyncStep, InSyncPass, FindNextSyncStep(InSyncStep)))
	{
		return true;
	}

	return false;
}

bool FTextureShareCoreObject::AcceptSyncBarrier(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject, const ETextureShareSyncStep InSyncStep, const ETextureShareSyncPass InSyncPass)
{
	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:AcceptSyncBarrier(%s[%s]) %s In << %s >>"), *GetName(), GetTEXT(InSyncStep), GetTEXT(InSyncPass), *ToString(LocalObject), *ToString(FrameConnections));

	if (LocalObject.Sync.AcceptSyncBarrier(InSyncStep, InSyncPass))
	{
		return true;
	}

	// debug purpose
	return false;
}

bool FTextureShareCoreObject::PrepareSyncBarrierPass(const ETextureShareSyncStep InSyncStep)
{
	bool bResult = false;

	// Return objects who support sync pass at this frame
	if (IsFrameSyncActive() && !FrameConnections.IsEmpty() && Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
	{
		if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
		{
			InterprocessMemory->UpdateFrameConnections(FrameConnections);
			bResult = InterprocessMemory->IsUsedSyncBarrierPass(FrameConnections, InSyncStep);
		}

		Owner.UnlockInterprocessMemory();
	}

	return bResult;
}

bool FTextureShareCoreObject::TryEnterSyncBarrier(const ETextureShareSyncStep InSyncStep) const
{
	switch (FrameSyncState)
	{
	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameBegin:
		if ((InSyncStep > ETextureShareSyncStep::FrameBegin) && (InSyncStep < ETextureShareSyncStep::FrameEnd))
		{
			if (GetSyncSetting().FrameSyncSettings.Contains(InSyncStep))
			{
				return true;
			}

			UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s: Unsupported Frame sync pass '%s' - Ignored - sync logic broken"), *GetName(), GetTEXT(InSyncStep));

			return false;
		}
		break;

	case ETextureShareCoreInterprocessObjectFrameSyncState::FrameProxyBegin:
		if ((InSyncStep > ETextureShareSyncStep::FrameProxyBegin) && (InSyncStep < ETextureShareSyncStep::FrameProxyEnd))
		{
			if (GetSyncSetting().FrameSyncSettings.Contains(InSyncStep))
			{
				return true;
			}

			UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s: Unsupported FrameProxy sync pass '%s' - Ignored - sync logic broken"), *GetName(), GetTEXT(InSyncStep));

			return false;
		}
		break;

	default:
		break;
	}

	UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:FrameSync(%s) - Skip sync step - %s"), *GetName(), GetTEXT(InSyncStep), GetTEXT(FrameSyncState));

	return false;
}

bool FTextureShareCoreObject::TryFrameProcessesBarrier(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject, const ETextureShareSyncStep InSyncStep, const ETextureShareSyncPass InSyncPass)
{
	// Mark last acces time for current process (this value used by other processes to detect die)
	LocalObject.Sync.UpdateLastAccessTime();

	if (FrameConnections.IsEmpty())
	{
		return false;
	}

#if TEXTURESHARECORE_DEBUGLOG
	InterprocessMemory.UpdateFrameConnections(FrameConnections);
	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:TryBarrier(%s) %s"), *GetName(), *ToString(LocalObject), *ToString(FrameConnections));
#endif

	const ETextureShareInterprocessObjectSyncBarrierState BarrierState = InterprocessMemory.GetBarrierState(LocalObject, FrameConnections);
	switch (BarrierState)
	{
	case ETextureShareInterprocessObjectSyncBarrierState::Accept:

		if (LocalObject.Sync.IsBarrierCompleted(InSyncStep, InSyncPass))
		{
			// break wait

			if (InSyncPass == ETextureShareSyncPass::Exit)
			{
				AcceptSyncBarrier(InterprocessMemory, LocalObject, InSyncStep, ETextureShareSyncPass::Complete);
			}

			return false;
		}

		// Accept barrier
		AcceptSyncBarrier(InterprocessMemory, LocalObject, InSyncStep, InSyncPass);

		// continue loop until barrier completed
		return true;

	case ETextureShareInterprocessObjectSyncBarrierState::Wait:
		// Continue wait
		return true;

	case ETextureShareInterprocessObjectSyncBarrierState::ObjectLost:
	case ETextureShareInterprocessObjectSyncBarrierState::FrameSyncLost:
	default:
		break;
	}

	HandleFrameLost(InterprocessMemory, LocalObject);

	// break wait
	return false;
}

bool FTextureShareCoreObject::SyncBarrierPass(const ETextureShareSyncStep InSyncStep, const ETextureShareSyncPass InSyncPass)
{
	bool bBarrierResult = false;
	if (IsFrameSyncActive() && Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
	{
		if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
		{
			if (FTextureShareCoreInterprocessObject* LocalObject = InterprocessMemory->FindObject(GetObjectDesc()))
			{
				// Begin barrier
				BeginSyncBarrier(*InterprocessMemory, *LocalObject, InSyncStep, InSyncPass);

				// Begin processs barrier sync
				FTextureShareCoreObjectTimeout FrameSyncTimer(SyncSettings.TimeoutSettings.FrameSyncTimeOut, SyncSettings.TimeoutSettings.FrameSyncTimeOutSplit);

				// repeat that barrier, until the connected processes is defined
				while (TryFrameProcessesBarrier(*InterprocessMemory, *LocalObject, InSyncStep, InSyncPass))
				{
					// Event error or timeout
					if (FrameSyncTimer.IsTimeOut())
					{
						HandleFrameLost(*InterprocessMemory, *LocalObject);
						break;
					}

					// Wait for remote process data changes
					if (!TryWaitFrameProcesses(FrameSyncTimer.GetRemainMaxMillisecondsToWait()))
					{
						HandleFrameLost(*InterprocessMemory, *LocalObject);

						return false;
					}
				}

				if (LocalObject->IsEnabled() && FrameConnections.Num() > 0)
				{
					bBarrierResult = true;
				}
			}
		}

		// Wake up remote processes anywait, because we change mem object header
		SendNotificationEvents(false);

		Owner.UnlockInterprocessMemory();
	}

	if (bBarrierResult)
	{
		return true;
	}

	UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:SyncBarrierPass  FAILED"), *GetName());

	return false;
}

ETextureShareSyncStep FTextureShareCoreObject::FindNextSyncStep(const ETextureShareSyncStep InSyncStep) const
{
	const TArray<ETextureShareSyncStep>& SyncSteps = SyncSettings.FrameSyncSettings.Steps;
	if (SyncSteps.IsEmpty() == false)
	{
		int32 Index = (InSyncStep == ETextureShareSyncStep::InterprocessConnection) ? 0 : SyncSteps.Find(InSyncStep);
		if (Index != INDEX_NONE)
		{
			while (++Index < SyncSteps.Num())
			{
				const ETextureShareSyncStep NextStep = SyncSteps[Index];
				switch (NextStep)
				{
				case ETextureShareSyncStep::FrameBegin:
				case ETextureShareSyncStep::FrameEnd:
				case ETextureShareSyncStep::FrameProxyBegin:
				case ETextureShareSyncStep::FrameProxyEnd:
				case ETextureShareSyncStep::Undefined:
					// Skip internal values
					break;
				default:
					if (FrameConnections.IsEmpty())
					{
						return NextStep;
					}

					// Use next step only if any process support
					for (const FTextureShareCoreObjectDesc& ObjIt : FrameConnections)
					{
						if (ObjIt.Sync.IsStepEnabled(NextStep))
						{
							return NextStep;
						}
					}
				}
			}

			// new frame connection
			return ETextureShareSyncStep::InterprocessConnection;
		}
	}

	return ETextureShareSyncStep::Undefined;
}
