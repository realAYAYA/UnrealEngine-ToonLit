// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertServerSequencerManager.h"
#include "IConcertSession.h"
#include "ConcertSyncServerLiveSession.h"
#include "ConcertWorkspaceMessages.h"

FConcertServerSequencerManager::FConcertServerSequencerManager(const TSharedRef<FConcertSyncServerLiveSession>& InLiveSession)
{
	BindSession(InLiveSession);
}

FConcertServerSequencerManager::~FConcertServerSequencerManager()
{
	UnbindSession();
}

void FConcertServerSequencerManager::BindSession(const TSharedRef<FConcertSyncServerLiveSession>& InLiveSession)
{
	check(InLiveSession->IsValidSession());

	UnbindSession();
	LiveSession = InLiveSession;

	LiveSession->GetSession().OnSessionClientChanged().AddRaw(this, &FConcertServerSequencerManager::HandleSessionClientChanged);
	LiveSession->GetSession().RegisterCustomEventHandler<FConcertSequencerCloseEvent>(this, &FConcertServerSequencerManager::HandleSequencerCloseEvent);
	LiveSession->GetSession().RegisterCustomEventHandler<FConcertSequencerStateEvent>(this, &FConcertServerSequencerManager::HandleSequencerStateEvent);
	LiveSession->GetSession().RegisterCustomEventHandler<FConcertSequencerOpenEvent>(this, &FConcertServerSequencerManager::HandleSequencerOpenEvent);
	LiveSession->GetSession().RegisterCustomEventHandler<FConcertSequencerTimeAdjustmentEvent>(this, &FConcertServerSequencerManager::HandleSequencerTimeAdjustmentEvent);
	LiveSession->GetSession().RegisterCustomEventHandler<FConcertWorkspaceSyncAndFinalizeCompletedEvent>(this, &FConcertServerSequencerManager::HandleWorkspaceSyncAndFinalizeCompletedEvent);
}

void FConcertServerSequencerManager::UnbindSession()
{
	if (LiveSession)
	{
		LiveSession->GetSession().OnSessionClientChanged().RemoveAll(this);
		LiveSession->GetSession().UnregisterCustomEventHandler<FConcertSequencerCloseEvent>(this);
		LiveSession->GetSession().UnregisterCustomEventHandler<FConcertSequencerStateEvent>(this);
		LiveSession->GetSession().UnregisterCustomEventHandler<FConcertSequencerOpenEvent>(this);
		LiveSession->GetSession().UnregisterCustomEventHandler<FConcertSequencerTimeAdjustmentEvent>(this);
		LiveSession->GetSession().UnregisterCustomEventHandler<FConcertWorkspaceSyncAndFinalizeCompletedEvent>(this);

		LiveSession.Reset();
	}
}

void FConcertServerSequencerManager::HandleSequencerStateEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerStateEvent& InEvent)
{
	// Create or update the Sequencer state 
	FConcertOpenSequencerState& SequencerState = SequencerStates.FindOrAdd(*InEvent.State.SequenceObjectPath);
	SequencerState.ClientEndpointIds.AddUnique(InEventContext.SourceEndpointId);
	SequencerState.State = InEvent.State;

	// Forward the message to the other clients
	TArray<FGuid> ClientIds = LiveSession->GetSession().GetSessionClientEndpointIds();
	ClientIds.Remove(InEventContext.SourceEndpointId);
	LiveSession->GetSession().SendCustomEvent(InEvent, ClientIds, EConcertMessageFlags::ReliableOrdered | EConcertMessageFlags::UniqueId);
}

void FConcertServerSequencerManager::HandleSequencerOpenEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerOpenEvent& InEvent)
{
	// Create or update the Sequencer state 
	FConcertOpenSequencerState& SequencerState = SequencerStates.FindOrAdd(*InEvent.SequenceObjectPath);
	SequencerState.ClientEndpointIds.AddUnique(InEventContext.SourceEndpointId);
	SequencerState.State.SequenceObjectPath = InEvent.SequenceObjectPath;

	// Forward the message to the other clients
	TArray<FGuid> ClientIds = LiveSession->GetSession().GetSessionClientEndpointIds();
	ClientIds.Remove(InEventContext.SourceEndpointId);
	LiveSession->GetSession().SendCustomEvent(InEvent, ClientIds, EConcertMessageFlags::ReliableOrdered | EConcertMessageFlags::UniqueId);
}

void FConcertServerSequencerManager::HandleSequencerTimeAdjustmentEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerTimeAdjustmentEvent& InEvent)
{
	// Verify that we have sequencers with the given SequenceObjectPath open.
	//
	FConcertOpenSequencerState* SequencerState = SequencerStates.Find(*InEvent.SequenceObjectPath);
	if (SequencerState)
	{
		// Forward the message to the other clients
		TArray<FGuid> ClientIds = LiveSession->GetSession().GetSessionClientEndpointIds();
		ClientIds.Remove(InEventContext.SourceEndpointId);
		LiveSession->GetSession().SendCustomEvent(InEvent, ClientIds, EConcertMessageFlags::ReliableOrdered | EConcertMessageFlags::UniqueId);
	}
}

void FConcertServerSequencerManager::HandleSequencerCloseEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerCloseEvent& InEvent)
{
	FConcertOpenSequencerState* SequencerState = SequencerStates.Find(*InEvent.SequenceObjectPath);
	if (SequencerState)
	{
		SequencerState->ClientEndpointIds.Remove(InEventContext.SourceEndpointId);
		// Forward a normal close event to clients
		const int32 NumOpen = SequencerState->ClientEndpointIds.Num();
		FConcertSequencerCloseEvent CloseEvent;
		CloseEvent.bControllerClose = NumOpen != 0 && InEvent.bControllerClose;
		CloseEvent.EditorsWithSequencerOpened = NumOpen;
		CloseEvent.SequenceObjectPath = InEvent.SequenceObjectPath;
		LiveSession->GetSession().SendCustomEvent(CloseEvent, LiveSession->GetSession().GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered | EConcertMessageFlags::UniqueId);
		if (NumOpen == 0)
		{
			SequencerStates.Remove(*InEvent.SequenceObjectPath);
		}
	}
}

void FConcertServerSequencerManager::HandleWorkspaceSyncAndFinalizeCompletedEvent(const FConcertSessionContext& InEventContext, const FConcertWorkspaceSyncAndFinalizeCompletedEvent& InEvent)
{
	FConcertSequencerStateSyncEvent SequencerStateSyncEvent;
	for (const auto& Pair : SequencerStates)
	{
		SequencerStateSyncEvent.SequencerStates.Add(Pair.Value.State);
	}

	LiveSession->GetSession().SendCustomEvent(SequencerStateSyncEvent, InEventContext.SourceEndpointId, EConcertMessageFlags::ReliableOrdered);
}

void FConcertServerSequencerManager::HandleSessionClientChanged(IConcertServerSession& InSession, EConcertClientStatus InClientStatus, const FConcertSessionClientInfo& InClientInfo)
{
	check(&InSession == &LiveSession->GetSession());
	// Remove the client from all open sequences
	if (InClientStatus == EConcertClientStatus::Disconnected)
	{
		for (auto It = SequencerStates.CreateIterator(); It; ++It)
		{
			It->Value.ClientEndpointIds.Remove(InClientInfo.ClientEndpointId);
			const int32 NumOpen = It->Value.ClientEndpointIds.Num();
			// Forward the close event to clients
			FConcertSequencerCloseEvent CloseEvent;
			CloseEvent.EditorsWithSequencerOpened = NumOpen;
			CloseEvent.SequenceObjectPath = It->Key.ToString();
			LiveSession->GetSession().SendCustomEvent(CloseEvent, LiveSession->GetSession().GetSessionClientEndpointIds(), EConcertMessageFlags::ReliableOrdered|EConcertMessageFlags::UniqueId);

			if (NumOpen == 0)
			{
				It.RemoveCurrent();
			}
		}
	}

	// Newly connected clients won't be sent the Sequencer state sync event
	// until they have synced and finalized their workspace since an open
	// sequence could have been created by a transaction in the activity
	// stream.
}
