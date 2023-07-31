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
bool FTextureShareCoreObject::TryFrameProcessesConnection(FTextureShareCoreInterprocessMemory& InterprocessMemory, FTextureShareCoreInterprocessObject& LocalObject)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TextureShareCore::Connect);

	// Mark last acces time for current process (this value used by other processes to detect die)
	LocalObject.Sync.UpdateLastAccessTime();

	// Collect valid processes to connect
	const int32 ReadyToConnectObjectsCount = InterprocessMemory.FindConnectableObjects(FrameConnections, LocalObject);

	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:TryConnect(%s)=%d %s"), *GetName(), *ToString(LocalObject), ReadyToConnectObjectsCount, *ToString(FrameConnections));

	// Handle sync logic
	const ETextureShareCoreFrameConnectionsState ConnectionsState = SyncSettings.FrameConnectionSettings.GetConnectionsState(ReadyToConnectObjectsCount, FrameConnections.Num());
	switch (ConnectionsState)
	{
	case ETextureShareCoreFrameConnectionsState::SkipFrame:
	default:
		// No available processes for connect. just skip this frame
		HandleFrameSkip(InterprocessMemory, LocalObject);

		// Reset first connect timeout when no processes
		bIsFrameConnectionTimeoutReached = false;

		// Break wait loop
		return false;

	case ETextureShareCoreFrameConnectionsState::Accept:

		// Reset first connect timeout after each success
		bIsFrameConnectionTimeoutReached = false;

		if (LocalObject.Sync.IsBarrierCompleted(ETextureShareSyncStep::InterprocessConnection, ETextureShareSyncPass::Enter))
		{
			SetFrameSyncState(ETextureShareCoreInterprocessObjectFrameSyncState::FrameConnected);

			// Break wait loop
			return false;
		}

		// Accept barrier
		AcceptSyncBarrier(InterprocessMemory, LocalObject, ETextureShareSyncStep::InterprocessConnection, ETextureShareSyncPass::Enter);

		// Continue this loop until EnterCompleted
		return true;

	case ETextureShareCoreFrameConnectionsState::Wait:
		if (bIsFrameConnectionTimeoutReached)
		{
			// after first timeout skip wait
			HandleFrameSkip(InterprocessMemory, LocalObject);
			return false;
		}

		// wait
		break;
	}

	// Reset connections list
	FrameConnections.Reset();

	// Wait for new frame connection
	return true;
}

bool FTextureShareCoreObject::ConnectFrameProcesses()
{
	UE_TS_LOG(LogTextureShareCoreObjectSync, Log, TEXT("%s:ConnectFrameProcesses()"), *GetName());

	FrameConnections.Reset();

	if (IsSessionActive() && IsActive() && Owner.LockInterprocessMemory(SyncSettings.TimeoutSettings.MemoryMutexTimeout))
	{
		if (FTextureShareCoreInterprocessMemory* InterprocessMemory = Owner.GetInterprocessMemory())
		{
			if (FTextureShareCoreInterprocessObject* LocalObject = InterprocessMemory->FindObject(GetObjectDesc()))
			{
				UpdateProcessId(*LocalObject);

				SetCurrentSyncStep(ETextureShareSyncStep::InterprocessConnection);
				SetFrameSyncState(ETextureShareCoreInterprocessObjectFrameSyncState::NewFrame);

				// Enter new frame sync barrier
				if (BeginSyncBarrier(*InterprocessMemory, *LocalObject, ETextureShareSyncStep::InterprocessConnection, ETextureShareSyncPass::Enter))
				{
					// Begin connect logic:
					FTextureShareCoreObjectTimeout FrameBeginTimer(SyncSettings.TimeoutSettings.FrameBeginTimeOut, SyncSettings.TimeoutSettings.FrameBeginTimeOutSplit);
					while (TryFrameProcessesConnection(*InterprocessMemory, *LocalObject))
					{
						if (FrameBeginTimer.IsTimeOut())
						{
							// Event error or timeout
							bIsFrameConnectionTimeoutReached = true;
							HandleFrameLost(*InterprocessMemory, *LocalObject);
							break;
						}

						// Wait for remote process data changes
						if (!TryWaitFrameProcesses(FrameBeginTimer.GetRemainMaxMillisecondsToWait()))
						{
							HandleFrameLost(*InterprocessMemory, *LocalObject);

							return false;
						}
					}
				}
			}
		}

		// Wake up remote processes anywait, because we change mem object header
		SendNotificationEvents(false);

		Owner.UnlockInterprocessMemory();
	}

	if (!FrameConnections.IsEmpty())
	{

		// Wait for other processes finish frame connect
		if (SyncBarrierPass(ETextureShareSyncStep::InterprocessConnection, ETextureShareSyncPass::Exit))
		{
			return true;
		}

		UE_TS_LOG(LogTextureShareCoreObjectSync, Error, TEXT("%s:ConnectFrameProcesses return FAILED (Exit barrier)"), *GetName());

		return false;
	}

	return false;
}

bool FTextureShareCoreObject::DisconnectFrameProcesses()
{
	return true;
}
