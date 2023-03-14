// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Serialization/BitReader.h"
#include "Misc/NetworkGuid.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"
#include "Misc/EngineVersion.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Misc/NetworkVersion.h"
#include "NetworkReplayStreaming.h"
#include "Engine/DemoNetConnection.h"
#include "Net/RepLayout.h"
#include "Net/Core/Connection/NetResult.h"
#include "Net/ReplayResult.h"
#include "Templates/Atomic.h"
#include "Net/UnrealNetwork.h"
#include "ReplayHelper.h"

#include "DemoNetDriver.generated.h"

class FNetworkNotify;
class FRepState;
class UDemoNetDriver;

DECLARE_MULTICAST_DELEGATE(FOnGotoTimeMCDelegate);
DECLARE_DELEGATE_OneParam(FOnGotoTimeDelegate, const bool /* bWasSuccessful */);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDemoStartedDelegate, UDemoNetDriver* /* DemoNetDriver */);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
UE_DEPRECATED(5.1, "No longer used")
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDemoFailedToStartDelegate, UDemoNetDriver* /* DemoNetDriver */, EDemoPlayFailure::Type /* FailureType*/);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

DECLARE_MULTICAST_DELEGATE(FOnDemoFinishPlaybackDelegate);
DECLARE_MULTICAST_DELEGATE(FOnDemoFinishRecordingDelegate);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPauseChannelsDelegate, const bool /* bPaused */);

class FQueuedReplayTask : public TSharedFromThis<FQueuedReplayTask>
{
public:
	FQueuedReplayTask(UDemoNetDriver* InDriver) : Driver(InDriver)
	{
	}

	virtual ~FQueuedReplayTask()
	{
	}

	virtual void StartTask() = 0;
	virtual bool Tick() = 0;
	virtual FName GetName() const = 0;
	virtual bool ShouldPausePlayback() const { return true; }

	TWeakObjectPtr<UDemoNetDriver> Driver;
};

/** Information about net startup actors that need to be rolled back by being destroyed and re-created */
USTRUCT()
struct FRollbackNetStartupActorInfo
{
	GENERATED_BODY()

	FName		Name;
	UPROPERTY()
	TObjectPtr<UObject>	Archetype = nullptr;
	FVector		Location;
	FRotator	Rotation;
	FVector		Scale3D;
	FName		LevelName;

	TSharedPtr<FRepState> RepState;
	TMap<FString, TSharedPtr<FRepState>> SubObjRepState;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> ObjReferences;

	void CountBytes(FArchive& Ar) const
	{
		if (FRepState const * const LocalRepState = RepState.Get())
		{
			Ar.CountBytes(sizeof(FRepState), sizeof(FRepState));
			LocalRepState->CountBytes(Ar);
		}

		SubObjRepState.CountBytes(Ar);
		for (const auto& SubObjRepStatePair : SubObjRepState)
		{
			SubObjRepStatePair.Key.CountBytes(Ar);
			
			if (FRepState const * const LocalRepState = SubObjRepStatePair.Value.Get())
			{
				const SIZE_T SizeOfRepState = sizeof(FRepState);
				Ar.CountBytes(SizeOfRepState, SizeOfRepState);
				LocalRepState->CountBytes(Ar);
			}
		}

		ObjReferences.CountBytes(Ar);
	}
};

struct ENGINE_API FDemoSavedRepObjectState
{
	FDemoSavedRepObjectState(
		const TWeakObjectPtr<const UObject>& InObject,
		const TSharedRef<const FRepLayout>& InRepLayout,
		FRepStateStaticBuffer&& InPropertyData);

	~FDemoSavedRepObjectState();

	TWeakObjectPtr<const UObject> Object;
	TSharedPtr<const FRepLayout> RepLayout;
	FRepStateStaticBuffer PropertyData;

	void CountBytes(FArchive& Ar) const
	{
		// The RepLayout for this object should still be stored by the UDemoNetDriver,
		// so we don't need to count it here.

		PropertyData.CountBytes(Ar);
	}
};

typedef TArray<struct FDemoSavedRepObjectState> FDemoSavedPropertyState;

USTRUCT()
struct FMulticastRecordOptions
{
	GENERATED_BODY()

	UPROPERTY()
	FString FuncPathName;

	UPROPERTY()
	bool bServerSkip = false;

	UPROPERTY()
	bool bClientSkip = false;
};

/**
 * Simulated network driver for recording and playing back game sessions.
 */
UCLASS(transient, config=Engine)
class ENGINE_API UDemoNetDriver : public UNetDriver
{
	GENERATED_BODY()

public:
	UDemoNetDriver(const FObjectInitializer& ObjectInitializer);
	UDemoNetDriver(FVTableHelper& Helper);
	~UDemoNetDriver();

	virtual void SetWorld(class UWorld* InWorld) override;

	/** Current record/playback frame number */
	int32 GetDemoFrameNum() const { return ReplayHelper.DemoFrameNum; }

	bool GetChannelsArePaused() const { return bChannelsArePaused; }

	double GetCurrentLevelIndex() const { return ReplayHelper.CurrentLevelIndex; }

	void SetCurrentLevelIndex(int32 Index)
	{
		ReplayHelper.CurrentLevelIndex = Index;
	}

	APlayerController* GetSpectatorController() const { return SpectatorController; }

private:

	/** This is the main spectator controller that is used to view the demo world from */
	APlayerController* SpectatorController;

	/** Our network replay streamer */
	TSharedPtr<class INetworkReplayStreamer> ReplayStreamer;

public:
	TSharedPtr<class INetworkReplayStreamer> GetReplayStreamer() const { return ReplayStreamer; }

	uint32 GetDemoCurrentTimeInMS() const { return (uint32)((double)GetDemoCurrentTime() * 1000); }

	/** Internal debug timing/tracking */
	double AccumulatedRecordTime;
	double LastRecordAvgFlush;
	double MaxRecordTime;
	int32 RecordCountSinceFlush;

private:
	/** 
	 * Net startup actors that need to be rolled back during scrubbing by being destroyed and re-spawned 
	 * NOTE - DeletedNetStartupActors will take precedence here, and destroy the actor instead
	 */
	UPROPERTY(transient)
	TMap<FString, FRollbackNetStartupActorInfo> RollbackNetStartupActors;

public:
	double GetLastCheckpointTime() const { return ReplayHelper.LastCheckpointTime; }

	void SetLastCheckpointTime(double CheckpointTime)
	{
		ReplayHelper.LastCheckpointTime = CheckpointTime;
	}

	void RespawnNecessaryNetStartupActors(TArray<AActor*>& SpawnedActors, ULevel* Level = nullptr);

private:

	void InitDefaults();

	bool LoadCheckpoint(const FGotoResult& GotoResult);
	
	TArray<TUniquePtr<FDeltaCheckpointData>> PlaybackDeltaCheckpointData;
	
	TSharedPtr<struct FReplayPlaylistTracker> PlaylistTracker;

public:

	void SetPlayingPlaylist(TSharedPtr<struct FReplayPlaylistTracker> InPlaylistTracker)
	{
		PlaylistTracker = InPlaylistTracker;
	}

	virtual void Serialize(FArchive& Ar) override;

	/** Returns true if we're in the process of saving a checkpoint. */
	bool IsSavingCheckpoint() const;
	bool IsLoadingCheckpoint() const { return ReplayHelper.bIsLoadingCheckpoint; }
	
	bool IsPlayingClientReplay() const;

	/** PlaybackPackets are used to buffer packets up when we read a demo frame, which we can then process when the time is right */
	TArray<FPlaybackPacket> PlaybackPackets;

	bool IsRecordingMapChanges() const { return ReplayHelper.bRecordMapChanges; }

	void RequestCheckpoint();

private:
	struct FDemoActorPriority
	{
		FActorPriority ActorPriority;
		UObject*	Level;
	};

	bool bIsFastForwarding;
	bool bIsFinalizingFastForward;
	bool bIsRestoringStartupActors;

	/** True if as have paused all of the channels */
	uint8 bChannelsArePaused : 1;
	uint8 bIsFastForwardingForCheckpoint : 1;
	uint8 bWasStartStreamingSuccessful : 1;

	/** If true, recording will prioritize replicating actors based on the value that AActor::GetReplayPriority returns. */
	uint8 bPrioritizeActors : 1;

protected:
	uint8 bIsWaitingForHeaderDownload : 1;
	uint8 bIsWaitingForStream : 1;

	TOptional<UE::Net::TNetResult<EReplayResult>> PendingRecordFailure;

private:
	TArray<FNetworkGUID> NonQueuedGUIDsForScrubbing;

	// Replay tasks
	TArray<TSharedRef<FQueuedReplayTask>>		QueuedReplayTasks;
	TSharedPtr<FQueuedReplayTask>				ActiveReplayTask;
	TSharedPtr<FQueuedReplayTask>				ActiveScrubReplayTask;

	/** Set via GotoTimeInSeconds, only fired once (at most). Called for successful or failed scrub. */
	FOnGotoTimeDelegate OnGotoTimeDelegate_Transient;
	
	/** Saved server time after loading a checkpoint, so that we can set the server time as accurately as possible after the fast-forward */
	float SavedReplicatedWorldTimeSeconds;

	/** Saved fast-forward time, used for correcting world time after the fast-forward is complete */
	float SavedSecondsToSkip;

	/** The unique identifier for the lifetime of this object. */
	FString DemoSessionID;

	/** Optional time quota for actor replication during recording. Going over this limit effectively lowers the net update frequency of the remaining actors. Negative values are considered unlimited. */
	float MaxDesiredRecordTimeMS;

	/**
	 * Maximum time allowed each frame to spend on saving a checkpoint. If 0, it will save the checkpoint in a single frame, regardless of how long it takes.
	 * See also demo.CheckpointSaveMaxMSPerFrameOverride.
	 */
	UPROPERTY(Config)
	float CheckpointSaveMaxMSPerFrame;

	/** A player controller that this driver should consider its viewpoint for actor prioritization purposes. */
	TWeakObjectPtr<APlayerController> ViewerOverride;

	/** Array of prioritized actors, used in TickDemoRecord. Stored as a member so that its storage doesn't have to be re-allocated each frame. */
	TArray<FDemoActorPriority> PrioritizedActors;

	/** Does the actual work of TickFlush, either on the main thread or in a task thread in parallel with Slate. */
	void TickFlushInternal(float DeltaSeconds);

	/** Returns the last checkpoint time in integer milliseconds. */
	uint32 GetLastCheckpointTimeInMS() const { return ReplayHelper.GetLastCheckpointTimeInMS(); }

	/** Called during a normal demoFrame*/
	void TickDemoRecordFrame(float DeltaSeconds);

	/** Config data for multicast RPCs we might want to skip recording. */
	UPROPERTY(config)
	TArray<FMulticastRecordOptions> MulticastRecordOptions;

public:

	// UNetDriver interface.

	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void FinishDestroy() override;
	virtual FString LowLevelGetNetworkNumber() override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& ListenURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickFlush(float DeltaSeconds) override;
	virtual void PostTickFlush() override;
	virtual void TickDispatch(float DeltaSeconds) override;
	virtual void ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = nullptr) override;
	virtual bool IsAvailable() const override { return true; }
	void SkipTime(const float InTimeToSkip);
	void SkipTimeInternal(const float SecondsToSkip, const bool InFastForward, const bool InIsForCheckpoint);
	bool InitConnectInternal(FString& Error);
	virtual bool ShouldClientDestroyTearOffActors() const override;
	virtual bool ShouldSkipRepNotifies() const override;
	virtual bool ShouldQueueBunchesForActorGUID(FNetworkGUID InGUID) const override;
	virtual bool ShouldIgnoreRPCs() const override;
	virtual FNetworkGUID GetGUIDForActor(const AActor* InActor) const override;
	virtual AActor* GetActorForGUID(FNetworkGUID InGUID) const override;
	virtual bool ShouldReceiveRepNotifiesForObject(UObject* Object) const override;
	virtual void ForceNetUpdate(AActor* Actor) override;
	virtual bool IsServer() const override;
	virtual bool ShouldReplicateFunction(AActor* Actor, UFunction* Function) const override;
	virtual bool ShouldReplicateActor(AActor* Actor) const override;
	virtual bool ShouldForwardFunction(AActor* Actor, UFunction* Function, void* Parms) const override;
	virtual void NotifyActorChannelOpen(UActorChannel* Channel, AActor* Actor) override;
	virtual void NotifyActorChannelCleanedUp(UActorChannel* Channel, EChannelCloseReason CloseReason) override;
	virtual void NotifyActorClientDormancyChanged(AActor* Actor, ENetDormancy OldDormancyState) override;
	virtual void NotifyActorTornOff(AActor* Actor) override;

	virtual void ProcessLocalServerPackets() override {}
	virtual void ProcessLocalClientPackets() override {}

	virtual void InitDestroyedStartupActors() override;

	virtual void SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InProvider) override;

	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override {}
	virtual class ISocketSubsystem* GetSocketSubsystem() override { return nullptr; }

	virtual bool DoesSupportEncryption() const override { return false; }

protected:
	virtual UChannel* InternalCreateChannelByName(const FName& ChName) override;

public:
	/** Called when we are already recording but have traveled to a new map to start recording again */
	bool ContinueListen(FURL& ListenURL);

	/** 
	 * Scrubs playback to the given time. 
	 * 
	 * @param TimeInSeconds
	 * @param InOnGotoTimeDelegate		Delegate to call when finished. Will be called only once at most.
	*/
	void GotoTimeInSeconds(const float TimeInSeconds, const FOnGotoTimeDelegate& InOnGotoTimeDelegate = FOnGotoTimeDelegate());

	bool IsRecording() const;
	bool IsPlaying() const;

	/** Total time of demo in seconds */
	float GetDemoTotalTime() const { return ReplayHelper.DemoTotalTime; }

	void SetDemoTotalTime(float TotalTime)
	{
		ReplayHelper.DemoTotalTime = TotalTime;
	}

	/** Current record/playback position in seconds */
	float GetDemoCurrentTime() const { return ReplayHelper.DemoCurrentTime; }

	void SetDemoCurrentTime(float CurrentTime)
	{
		ReplayHelper.DemoCurrentTime = CurrentTime;
	}

	FString GetDemoURL() const { return ReplayHelper.DemoURL.ToString(); }

	/** Sets the desired maximum recording time in milliseconds. */
	void SetMaxDesiredRecordTimeMS(const float InMaxDesiredRecordTimeMS) { MaxDesiredRecordTimeMS = InMaxDesiredRecordTimeMS; }

	float GetMaxDesiredRecordTimeMS() const { return MaxDesiredRecordTimeMS; }

	/** Sets the controller to use as the viewpoint for recording prioritization purposes. */
	void SetViewerOverride(APlayerController* const InViewerOverride ) { ViewerOverride = InViewerOverride; }

	/** Enable or disable prioritization of actors for recording. */
	void SetActorPrioritizationEnabled(const bool bInPrioritizeActors) { bPrioritizeActors = bInPrioritizeActors; }

	bool IsActorPrioritizationEnabled() const { return bPrioritizeActors; }

	/** Sets CheckpointSaveMaxMSPerFrame. */
	void SetCheckpointSaveMaxMSPerFrame(const float InCheckpointSaveMaxMSPerFrame)
	{ 
		CheckpointSaveMaxMSPerFrame = InCheckpointSaveMaxMSPerFrame; 
		ReplayHelper.CheckpointSaveMaxMSPerFrame = InCheckpointSaveMaxMSPerFrame;
	}

	/** Called by a task thread if the engine is doing async end of frame tasks in parallel with Slate. */
	void TickFlushAsyncEndOfFrame(float DeltaSeconds);

	const TArray<FLevelNameAndTime>& GetLevelNameAndTimeList();

	/** Returns the replicated state of every object on a current actor channel. Use the result to compare in DiffReplicatedProperties. */
	FDemoSavedPropertyState SavePropertyState() const;

	/** Compares the values of replicated properties stored in State with the current values of the object replicators. Logs and returns true if there were any differences. */
	bool ComparePropertyState(const FDemoSavedPropertyState& State) const;

public:
	/** @return true if the net resource is valid or false if it should not be used */
	virtual bool IsNetResourceValid(void) override { return true; }

	void TickDemoRecord(float DeltaSeconds);
	void PauseChannels(const bool bPause);
	void PauseRecording(const bool bInPauseRecording);
	bool IsRecordingPaused() const;

	bool ConditionallyProcessPlaybackPackets();
	void ProcessAllPlaybackPackets();

	bool ConditionallyReadDemoFrameIntoPlaybackPackets(FArchive& Ar);

	bool ProcessPacket(const uint8* Data, int32 Count);
	bool ProcessPacket(const FPlaybackPacket& PlaybackPacket)
	{
		return ShouldSkipPlaybackPacket(PlaybackPacket) ||
				ProcessPacket(PlaybackPacket.Data.GetData(), PlaybackPacket.Data.Num());
	}
	
	void WriteDemoFrameFromQueuedDemoPackets(FArchive& Ar, TArray<FQueuedDemoPacket>& QueuedPackets, float FrameTime, EWriteDemoFrameFlags Flags);

	void WritePacket(FArchive& Ar, uint8* Data, int32 Count);

	void TickDemoPlayback(float DeltaSeconds);
	
	void FinalizeFastForward(const double StartTime);
	
	void SpawnDemoRecSpectator( UNetConnection* Connection, const FURL& ListenURL);

	/**
	 * Restores the given player controller so that it properly points to the given NetConnection
	 * after scrubbing when viewing a replay.
	 *
	 * @param PC			The PlayerController to set up the given NetConnection for
	 * @param NetConnection	The NetConnection to be assigned to the player controller.
	 */
	void RestoreConnectionPostScrub(APlayerController* PC, UNetConnection* NetConnection);

	/**
	 * Sets the main spectator controller to be used and adds them to the spectator control array
	 *
	 * @param PC			The PlayerController to set the main controller param to.
	 */
	void SetSpectatorController(APlayerController* PC);
	
	// Splitscreen demo handling

	/**
	 * Creates a new splitscreen replay viewer.
	 *
	 * @param NewPlayer		The LocalPlayer in control of this new viewer
	 * @param InWorld		The world to spawn the new viewer in.
	 *
	 * @return If the viewer was able to be created or not.
	 */
	bool SpawnSplitscreenViewer(ULocalPlayer* NewPlayer, UWorld* InWorld);

	/**
	 * Removes a splitscreen demo viewer and cleans up its connection.
	 *
	 * @param RemovePlayer		The PlayerController to remove from the replay system
	 * @param bMarkOwnerForDeletion		If this function should handle deleting the given player as well.
	 *
	 * @return If the player was successfully removed from the replay.
	 */
	bool RemoveSplitscreenViewer(APlayerController* RemovePlayer, bool bMarkOwnerForDeletion=false);

private:
	// Internal splitscreen management

	/** An array of all the spectator controllers (the main one and all splitscreen ones) that currently exist */
	UPROPERTY(transient)
	TArray<TObjectPtr<APlayerController>> SpectatorControllers;

	/**
	 * Removes all child connections for splitscreen viewers.
	 * This should be done before the ClientConnections or ServerConnection
	 * variables change or during most travel scenarios.
	 *
	 * @param bDeleteOwner	If the connections should delete the owning actor to the connection
	 *
	 * @return The number of splitscreen connections cleaned up.
	 */
	int32 CleanUpSplitscreenConnections(bool bDeleteOwner);

public:

	void ResetDemoState();
	void JumpToEndOfLiveReplay();
	void AddEvent(const FString& Group, const FString& Meta, const TArray<uint8>& Data);
	void AddOrUpdateEvent(const FString& EventName, const FString& Group, const FString& Meta, const TArray<uint8>& Data);

	void EnumerateEvents(const FString& Group, const FEnumerateEventsCallback& Delegate);

	// In most cases, this is desirable over EnumerateEvents because it will explicitly use ActiveReplayName
	// instead of letting the streamer decide.
	void EnumerateEventsForActiveReplay(const FString& Group, const FEnumerateEventsCallback& Delegate);
	void EnumerateEventsForActiveReplay(const FString& Group, const int32 UserIndex, const FEnumerateEventsCallback& Delegate);

	void RequestEventData(const FString& EventID, const FRequestEventDataCallback& Delegate);

	// In most cases, this is desirable over RequestEventData because it will explicitly use ActiveReplayName
	// instead of letting the streamer decide.
	void RequestEventDataForActiveReplay(const FString& EventID, const FRequestEventDataCallback& Delegate);
	void RequestEventDataForActiveReplay(const FString& EventID, const int32 UserIndex, const FRequestEventDataCallback& Delegate);

	/** Retrieve data for all events matching the requested group, and call the passed in delegate on completion. */
	void RequestEventGroupDataForActiveReplay(const FString& Group, const FRequestEventGroupDataCallback& Delegate);
	void RequestEventGroupDataForActiveReplay(const FString& Group, const int32 UserIndex, const FRequestEventGroupDataCallback& Delegate);

	bool IsFastForwarding() const { return bIsFastForwarding; }
	bool IsFinalizingFastForward() const { return bIsFinalizingFastForward; }
	bool IsRestoringStartupActors() const { return bIsRestoringStartupActors; }

	FReplayExternalDataArray* GetExternalDataArrayForObject(UObject* Object);

	bool SetExternalDataForObject(UObject* OwningObject, const uint8* Src, const int32 NumBits);

	bool ReadDemoFrameIntoPlaybackPackets(FArchive& Ar, TArray<FPlaybackPacket>& Packets, const bool bForLevelFastForward, float* OutTime);
	bool ReadDemoFrameIntoPlaybackPackets(FArchive& Ar) { return ReadDemoFrameIntoPlaybackPackets(Ar, PlaybackPackets, false, nullptr); }

	/**
	 * Adds a join-in-progress user to the set of users associated with the currently recording replay (if any)
	 *
	 * @param UserString a string that uniquely identifies the user, usually their FUniqueNetId
	 */
	void AddUserToReplay(const FString& UserString);

	void StopDemo();

	void ReplayStreamingReady(const FStartStreamingResult& Result);

	void AddReplayTask(FQueuedReplayTask* NewTask);
	bool IsAnyTaskPending() const;
	void ClearReplayTasks();
	bool ProcessReplayTasks();
	bool IsNamedTaskInQueue(const FName& Name) const;
	FName GetNextQueuedTaskName() const;

	/** If a channel is associated with Actor, adds the channel's GUID to the list of GUIDs excluded from queuing bunches during scrubbing. */
	void AddNonQueuedActorForScrubbing(AActor const* Actor);
	/** Adds the channel's GUID to the list of GUIDs excluded from queuing bunches during scrubbing. */
	void AddNonQueuedGUIDForScrubbing(FNetworkGUID InGUID);

	virtual bool IsLevelInitializedForActor(const AActor* InActor, const UNetConnection* InConnection) const override;

	/** Called when a "go to time" operation is completed. */
	void NotifyGotoTimeFinished(bool bWasSuccessful);

	virtual void NotifyActorDestroyed(AActor* ThisActor, bool IsSeamlessTravel=false) override;
	virtual void NotifyActorLevelUnloaded(AActor* Actor) override;
	virtual void NotifyStreamingLevelUnload(ULevel* InLevel) override;

	/** Call this function during playback to track net startup actors that need a hard reset when scrubbing, which is done by destroying and then re-spawning */
	virtual void QueueNetStartupActorForRollbackViaDeletion(AActor* Actor);

	/** Called when seamless travel begins when recording a replay. */
	void OnSeamlessTravelStartDuringRecording(const FString& LevelName);

	/** @return the unique identifier for the lifetime of this object. */
	const FString& GetDemoSessionID() const { return DemoSessionID; }

	/** Returns true if TickFlush can be called in parallel with the Slate tick. */
	bool ShouldTickFlushAsyncEndOfFrame() const;

	/** Returns whether or not this replay was recorded / is playing with Level Streaming fixes. */
	bool HasLevelStreamingFixes() const
	{
		return ReplayHelper.HasLevelStreamingFixes();
	}

	/** Returns whether or not this replay was recorded / is playing with delta checkpoints. */
	bool HasDeltaCheckpoints() const 
	{
		return ReplayHelper.HasDeltaCheckpoints();
	}

	/** Returns whether or not this replay was recorded / is playing with the game specific per frame data feature. */
	bool HasGameSpecificFrameData() const
	{
		return ReplayHelper.HasGameSpecificFrameData();
	}

	/**
	 * Gets the actively recording or playback replay (stream) name.
	 * Note, this will be empty when not recording or playing back.
	 */
	const FString& GetActiveReplayName() const
	{
		return ReplayHelper.ActiveReplayName;
	}

	uint32 GetPlaybackDemoChangelist() const { return ReplayHelper.PlaybackDemoHeader.EngineVersion.GetChangelist(); }
	uint32 GetPlaybackDemoVersion() const { return ReplayHelper.PlaybackDemoHeader.Version; }

	uint32 GetPlaybackEngineNetworkProtocolVersion() const { return ReplayHelper.PlaybackDemoHeader.EngineNetworkProtocolVersion; }
	uint32 GetPlaybackGameNetworkProtocolVersion() const { return ReplayHelper.PlaybackDemoHeader.GameNetworkProtocolVersion; }

	FString GetDemoPath() const;

private:

	struct FLevelnterval
	{
		int32 Priority;
		int32 StartIndex;
		int32 Count;
		int32 LevelIndex;
	};

	TArray<FLevelnterval> LevelIntervals;

	void BuildSortedLevelPriorityOnLevels(const TArray<FDemoActorPriority>& PrioritizedActorList, TArray<FLevelnterval>& OutLevelIntervals);

	/** Called when the downloading header request from the replay streamer completes. */
	void OnRefreshHeaderCompletePrivate(const FDownloadHeaderResult& Result, int32 LevelIndex);

	void CleanupOutstandingRewindActors();

	// Tracks actors that will need to be rewound during scrubbing.
	// This list should always be empty outside of scrubbing.
	TSet<FNetworkGUID> TrackedRewindActorsByGUID;

	// Time of the last packet we've processed (in seconds).
	float LastProcessedPacketTime;

	// Index into PlaybackPackets array. Used so we can process many packets in one frame and avoid removing them individually.
	int32 PlaybackPacketIndex;

	// Determines whether or not a packet should be skipped, based on it's level association.
	bool ShouldSkipPlaybackPacket(const FPlaybackPacket& Packet);

	/**
	 * Replicates the given prioritized actors, so their packets can be captured for recording.
	 * This should be used for normal frame recording.
	 * @see ReplicateCheckpointActor for recording during checkpoints.
	 *
	 * @param ToReplicate	The actors to replicate.
	 * @param Params		Implementation specific params necessary to replicate the actor.
	 *
	 * @return True if there is time remaining to replicate more actors. False otherwise.
	 */
	bool ReplicatePrioritizedActors(const FDemoActorPriority* ActorsToReplicate, uint32 Count, class FRepActorsParams& Params);
	bool ReplicatePrioritizedActor(const FActorPriority& ActorPriority, class FRepActorsParams& Params);

	friend class FPendingTaskHelper;

	// Manages basic setup of newly visible levels, and queuing a FastForward task if necessary.
	void PrepFastForwardLevels();

	// Performs the logic for actually fast-forwarding a level.
	bool FastForwardLevels(const FGotoResult& GotoResult);

	void OnPostLoadMapWithWorld(UWorld* World);

	// Diff Actor plus it's Components and Subobjects from given ActorChannel
	void DiffActorProperties(UActorChannel* const ActorChannel);

	// Callback sent just before an actor has destroy called on itself.
	void OnActorPreDestroy(AActor* DestroyedActor);

	FDelegateHandle DelegateHandleActorPreDestroy;

protected:

	void ProcessSeamlessTravel(int32 LevelIndex);

	bool DemoReplicateActor(AActor* Actor, UNetConnection* Connection, bool bMustReplicate);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.1, "Please use NotifyDemoPlaybackError instead")
	void NotifyDemoPlaybackFailure(EDemoPlayFailure::Type FailureType);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	void NotifyDemoPlaybackError(const UE::Net::TNetResult<EReplayResult>& Result);
	void NotifyDemoRecordFailure(const UE::Net::TNetResult<EReplayResult>& Result);

	TArray<FQueuedDemoPacket> QueuedPacketsBeforeTravel;

	int64 MaxArchiveReadPos;

private:

	// Max percent of time to spend building consider lists / prioritizing actors
	// for demo recording. Only used if MaxDesiredRecordTimeMS > 0.
	float RecordBuildConsiderAndPrioritizeTimeSlice;

	// Max percent of time to spend replicating prioritized destruction infos. Only used if MaxDesiredRecordTimeMS > 0.
	float RecordDestructionInfoReplicationTimeSlice;

	void AdjustConsiderTime(const float ReplicatedPercent);

	bool ProcessFastForwardPackets(TArrayView<FPlaybackPacket> Packets, const TSet<int32>& LevelIndices);
	void ProcessPlaybackPackets(TArrayView<FPlaybackPacket> Packets);

	virtual ECreateReplicationChangelistMgrFlags GetCreateReplicationChangelistMgrFlags() const override
	{
		return ECreateReplicationChangelistMgrFlags::SkipDeltaCustomState;
	}

	TUniquePtr<struct FDemoBudgetLogHelper> BudgetLogHelper;

//////////////////////////////////////////////////////////////////////////
// Replay frame fidelity
public:
	// Simplified rating of replay frame fidelity as percentage of actors that were replicated.
	// [0..1] where 0 means nothing was recorded this frame and 1 means full fidelity.
	// This treats all actors equally. Assuming more important actors are prioritized higher, in general actual "fidelity"
	// is expected to be higher than reported, which should be fine for detecting low-fidelity frame/intervals in replay file.
	float GetLastReplayFrameFidelity() const
	{
		return LastReplayFrameFidelity;
	}

private:
	TAtomic<float> LastReplayFrameFidelity{ 0 };

	FReplayHelper ReplayHelper;

	/** Enabled via -skipreplayrollback and causes rollback data to not be generated, with the assumption that there will be no scrubbing. */
	bool bSkipStartupActorRollback = false;

	friend class UDemoNetConnection;
};