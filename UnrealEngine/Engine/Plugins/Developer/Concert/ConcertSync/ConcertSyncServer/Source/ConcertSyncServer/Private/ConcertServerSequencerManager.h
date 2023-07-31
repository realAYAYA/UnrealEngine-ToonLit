// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertMessages.h"
#include "ConcertSequencerMessages.h"
#include "ConcertWorkspaceMessages.h"

struct FConcertSessionContext;
class IConcertServerSession;
class FConcertSyncServerLiveSession;

class FConcertServerSequencerManager
{
public:
	explicit FConcertServerSequencerManager(const TSharedRef<FConcertSyncServerLiveSession>& InLiveSession);
	~FConcertServerSequencerManager();

	/** Bind this manager to the server session. */
	void BindSession(const TSharedRef<FConcertSyncServerLiveSession>& InLiveSession);

	/** Unbind the manager from its currently bound session. */
	void UnbindSession();

private:
	/** Handler for the sequencer state updated event. */
	void HandleSequencerStateEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerStateEvent& InEvent);

	/** Handler for the sequencer open event. */
	void HandleSequencerOpenEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerOpenEvent& InEvent);

	/** Handler for the sequencer close event. */
	void HandleSequencerCloseEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerCloseEvent& InEvent);

	/** Handler for the sequencer time adjustment event. */
	void HandleSequencerTimeAdjustmentEvent(const FConcertSessionContext& InEventContext, const FConcertSequencerTimeAdjustmentEvent& InEvent);

	/** Handler for the workspace sync and finalize completed event. */
	void HandleWorkspaceSyncAndFinalizeCompletedEvent(const FConcertSessionContext& InEventContext, const FConcertWorkspaceSyncAndFinalizeCompletedEvent& InEvent);

	/** Handler for the session clients changed event. */
	void HandleSessionClientChanged(IConcertServerSession& InSession, EConcertClientStatus InClientStatus, const FConcertSessionClientInfo& InClientInfo);

	struct FConcertOpenSequencerState
	{
		/** Client Endpoints that have this Sequence opened. */
		TArray<FGuid> ClientEndpointIds;

		/** Current state of the Sequence. */
		FConcertSequencerState State;
	};

	/** Map of all currently opened Sequencer in a session, locally opened or not. */
	TMap<FName, FConcertOpenSequencerState> SequencerStates;

	/** Live session tracked by this manager. */
	TSharedPtr<FConcertSyncServerLiveSession> LiveSession;
};
