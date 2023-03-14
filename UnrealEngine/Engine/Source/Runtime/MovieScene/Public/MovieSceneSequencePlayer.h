// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "IMovieScenePlayer.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "IMovieScenePlaybackClient.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneTimeController.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieScenePlayback.h"
#include "MovieSceneSequenceTickManagerClient.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "MovieSceneLatentActionManager.h"
#include "IMovieSceneSequencePlayerObserver.h"
#include "EntitySystem/MovieSceneEntityIDs.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
	#include "MovieSceneSequenceTickManager.h"
#endif

#include "MovieSceneSequencePlayer.generated.h"

class UMovieSceneSequenceTickManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMovieSceneSequencePlayerEvent);

/**
 * Enum used to define how to update to a particular time
 */
UENUM(BlueprintType)
enum class EUpdatePositionMethod : uint8
{
	/** Update from the current position to a specified position (including triggering events), using the current player status */
	Play,
	/** Jump to a specified position (without triggering events in between), using the current player status */
	Jump,
	/** Jump to a specified position, temporarily using EMovieScenePlayerStatus::Scrubbing */
	Scrub,
};


/**
 * Properties that are broadcast from server->clients for time/state synchronization
 */
USTRUCT()
struct FMovieSceneSequenceReplProperties
{
	GENERATED_BODY()

	FMovieSceneSequenceReplProperties()
		: LastKnownStatus(EMovieScenePlayerStatus::Stopped)
		, LastKnownNumLoops(0)
	{}

	/** The last known position of the sequence on the server */
	UPROPERTY()
	FFrameTime LastKnownPosition;

	/** The last known playback status of the sequence on the server */
	UPROPERTY()
	TEnumAsByte<EMovieScenePlayerStatus::Type> LastKnownStatus;

	/** The last known number of loops of the sequence on the server */
	UPROPERTY()
	int32 LastKnownNumLoops;
};


UENUM(BlueprintType)
enum class EMovieScenePositionType : uint8
{
	Frame,
	Time,
	MarkedFrame,
};

USTRUCT(BlueprintType)
struct FMovieSceneSequencePlaybackParams
{
	GENERATED_BODY()

	FMovieSceneSequencePlaybackParams()
		: Time(0.f)
		, PositionType(EMovieScenePositionType::Frame)
		, UpdateMethod(EUpdatePositionMethod::Play)
		, bHasJumped(false)
	{}

	FMovieSceneSequencePlaybackParams(FFrameTime InFrame, EUpdatePositionMethod InUpdateMethod)
		: Frame(InFrame)
		, Time(0.f)
		, PositionType(EMovieScenePositionType::Frame)
		, UpdateMethod(InUpdateMethod)
		, bHasJumped(false)
	{}

	FMovieSceneSequencePlaybackParams(float InTime, EUpdatePositionMethod InUpdateMethod)
		: Time(InTime)
		, PositionType(EMovieScenePositionType::Time)
		, UpdateMethod(InUpdateMethod)
		, bHasJumped(false)
	{}

	FMovieSceneSequencePlaybackParams(const FString& InMarkedFrame, EUpdatePositionMethod InUpdateMethod)
		: Time(0.f)
		, MarkedFrame(InMarkedFrame)
		, PositionType(EMovieScenePositionType::MarkedFrame)
		, UpdateMethod(InUpdateMethod)
		, bHasJumped(false)
	{}

	FFrameTime GetPlaybackPosition(UMovieSceneSequencePlayer* Player) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(EditCondition="PositionType == EMovieScenePositionType::Frame"))
	FFrameTime Frame;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(EditCondition="PositionType == EMovieScenePositionType::Time", unit=s))
	float Time;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(EditCondition="PositionType == EMovieScenePositionType::MarkedFrame"))
	FString MarkedFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	EMovieScenePositionType PositionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	EUpdatePositionMethod UpdateMethod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bHasJumped;
};

USTRUCT(BlueprintType)
struct FMovieSceneSequencePlayToParams
{
	GENERATED_BODY()

	/** Should the PlayTo time be considered exclusive? Defaults to true as end frames in Sequencer are exclusive by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bExclusive = true;
};

/**
 * Abstract class that provides consistent player behaviour for various animation players
 */
UCLASS(Abstract, BlueprintType)
class MOVIESCENE_API UMovieSceneSequencePlayer
	: public UObject
	, public IMovieScenePlayer
	, public IMovieSceneSequenceTickManagerClient
{
public:
	GENERATED_BODY()

	/** Obeserver interface used for controlling whether the effects of this sequence can be seen even when it is playing back. */
	UPROPERTY(replicated)
	TScriptInterface<IMovieSceneSequencePlayerObserver> Observer;

	UMovieSceneSequencePlayer(const FObjectInitializer&);
	virtual ~UMovieSceneSequencePlayer();

	/** Start playback forwards from the current time cursor position, using the current play rate. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void Play();

	/** Reverse playback. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void PlayReverse();

	/** Changes the direction of playback (go in reverse if it was going forward, or vice versa) */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void ChangePlaybackDirection();

	/**
	 * Start playback from the current time cursor position, looping the specified number of times.
	 * @param NumLoops - The number of loops to play. -1 indicates infinite looping.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void PlayLooping(int32 NumLoops = -1);
	
	/** Pause playback. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void Pause();
	
	/** Scrub playback. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void Scrub();

	/** Stop playback and move the cursor to the end (or start, for reversed playback) of the sequence. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void Stop();

	/** Stop playback without moving the cursor. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void StopAtCurrentTime();

	/** Go to end and stop. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", meta = (ToolTip = "Go to end of the sequence and stop. Adheres to 'When Finished' section rules."))
	void GoToEndAndStop();

public:

	/**
	 * Get the current playback position
	 * @return The current playback position
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	FQualifiedFrameTime GetCurrentTime() const;

	/**
	 * Get the total duration of the sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	FQualifiedFrameTime GetDuration() const;

	/**
	 * Get this sequence's duration in frames
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	int32 GetFrameDuration() const;

	/**
	 * Get this sequence's display rate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	FFrameRate GetFrameRate() const { return PlayPosition.GetInputRate(); }

	/**
	 * Set the frame-rate that this player should play with, making all frame numbers in the specified time-space
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void SetFrameRate(FFrameRate FrameRate);

	/**
	 * Get the offset within the level sequence to start playing
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	FQualifiedFrameTime GetStartTime() const { return FQualifiedFrameTime(StartTime, PlayPosition.GetInputRate()); }

	/**
	 * Get the offset within the level sequence to finish playing
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	FQualifiedFrameTime GetEndTime() const { return FQualifiedFrameTime(StartTime + DurationFrames, PlayPosition.GetInputRate()); }

public:

	/**
	 * Set the valid play range for this sequence, determined by a starting frame number (in this sequence player's plaback frame), and a number of frames duration
	 *
	 * @param StartFrame      The frame number to start playing back the sequence
	 * @param Duration        The number of frames to play
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", DisplayName="Set Play Range (Frames)")
	void SetFrameRange( int32 StartFrame, int32 Duration, float SubFrames = 0.f );

	/**
	 * Set the valid play range for this sequence, determined by a starting time  and a duration (in seconds)
	 *
	 * @param StartTime       The time to start playing back the sequence in seconds
	 * @param Duration        The length to play for
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", DisplayName="Set Play Range (Seconds)")
	void SetTimeRange( float StartTime, float Duration );

public:

	/**
	 * Play from the current position to the requested position and pause. If requested position is before the current position, 
	 * playback will be reversed. Playback to the requested position will be cancelled if Stop() or Pause() is invoked during this 
	 * playback.
	 *
	 * @param PlaybackParams The position settings (ie. the position to play to)
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void PlayTo(FMovieSceneSequencePlaybackParams PlaybackParams, FMovieSceneSequencePlayToParams PlayToParams);

	/**
	 * Set the current time of the player by evaluating from the current time to the specified time, as if the sequence is playing. 
	 * Triggers events that lie within the evaluated range. Does not alter the persistent playback status of the player (IsPlaying).
	 *
	 * @param PlaybackParams The position settings (ie. the position to set playback to)
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void SetPlaybackPosition(FMovieSceneSequencePlaybackParams PlaybackParams);

	/**
	 * Restore any changes made by this player to their original state
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void RestoreState();

public:

	UE_DEPRECATED(4.26, "PlayToFrame is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", DisplayName = "Play To (Frames)", meta=(DeprecatedFunction, DeprecationMessage="PlayToFrame is deprecated, use SetPlaybackPosition."))
	void PlayToFrame(FFrameTime NewPosition) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(NewPosition, EUpdatePositionMethod::Play)); }

	UE_DEPRECATED(4.26, "ScrubToFrame is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", DisplayName = "Scrub To (Frames)", meta=(DeprecatedFunction, DeprecationMessage="ScrubToFrame is deprecated, use SetPlaybackPosition."))
	void ScrubToFrame(FFrameTime NewPosition) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(NewPosition, EUpdatePositionMethod::Scrub)); }

	UE_DEPRECATED(4.26, "JumpToFrame is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", DisplayName="Jump To (Frames)", meta=(DeprecatedFunction, DeprecationMessage="JumpToFrame is deprecated, use SetPlaybackPosition."))
	void JumpToFrame(FFrameTime NewPosition) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(NewPosition, EUpdatePositionMethod::Jump)); }

	UE_DEPRECATED(4.26, "PlayToSeconds is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", DisplayName = "Play To (Seconds)", meta=(DeprecatedFunction, DeprecationMessage="PlayToSeconds is deprecated, use SetPlaybackPosition."))
	void PlayToSeconds(float TimeInSeconds) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(TimeInSeconds, EUpdatePositionMethod::Play)); }

	UE_DEPRECATED(4.26, "ScrubToSeconds is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", DisplayName = "Scrub To (Seconds)", meta=(DeprecatedFunction, DeprecationMessage="ScrubToSeconds is deprecated, use SetPlaybackPosition."))
	void ScrubToSeconds(float TimeInSeconds) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(TimeInSeconds, EUpdatePositionMethod::Scrub)); }

	UE_DEPRECATED(4.26, "JumpToSeconds is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", DisplayName = "Jump To (Seconds)", meta=(DeprecatedFunction, DeprecationMessage="JumpToSeconds is deprecated, use SetPlaybackPosition."))
	void JumpToSeconds(float TimeInSeconds) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(TimeInSeconds, EUpdatePositionMethod::Jump)); }

	UE_DEPRECATED(4.26, "PlayToMarkedFrame is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", meta=(DeprecatedFunction, DeprecationMessage="PlayToMarkedFrame is deprecated, use SetPlaybackPosition."))
	bool PlayToMarkedFrame(const FString& InLabel) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(InLabel, EUpdatePositionMethod::Play)); return true; }

	UE_DEPRECATED(4.26, "ScrubToMarkedFrame is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", meta=(DeprecatedFunction, DeprecationMessage="ScrubToMarkedFrame is deprecated, use SetPlaybackPosition."))
	bool ScrubToMarkedFrame(const FString& InLabel) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(InLabel, EUpdatePositionMethod::Scrub)); return true; }

	UE_DEPRECATED(4.26, "JumpToMarkedFrame is deprecated, use SetPlaybackPosition.")
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player", meta=(DeprecatedFunction, DeprecationMessage="JumpToMarkedFrame is deprecated, use SetPlaybackPosition."))
	bool JumpToMarkedFrame(const FString& InLabel) { SetPlaybackPosition(FMovieSceneSequencePlaybackParams(InLabel, EUpdatePositionMethod::Jump)); return true; }

public:

	/** Check whether the sequence is actively playing. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	bool IsPlaying() const;

	/** Check whether the sequence is paused. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	bool IsPaused() const;

	/** Check whether playback is reversed. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	bool IsReversed() const;

	/** Get the playback rate of this player. */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	float GetPlayRate() const;

	/**
	 * Set the playback rate of this player. Negative values will play the animation in reverse.
	 * @param PlayRate - The new rate of playback for the animation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void SetPlayRate(float PlayRate);

	/** Set whether to disable camera cuts */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	void SetDisableCameraCuts(bool bInDisableCameraCuts) { PlaybackSettings.bDisableCameraCuts = bInDisableCameraCuts; }

	/** Set whether to disable camera cuts */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	bool GetDisableCameraCuts() { return PlaybackSettings.bDisableCameraCuts; }

	/** An event that is broadcast each time this level sequence player is updated */
	DECLARE_EVENT_ThreeParams( UMovieSceneSequencePlayer, FOnMovieSceneSequencePlayerUpdated, const UMovieSceneSequencePlayer&, FFrameTime /*current time*/, FFrameTime /*previous time*/ );
	FOnMovieSceneSequencePlayerUpdated& OnSequenceUpdated() const { return OnMovieSceneSequencePlayerUpdate; }

	/** Event triggered when the level sequence player is played */
	UPROPERTY(BlueprintAssignable, Category = "Sequencer|Player")
	FOnMovieSceneSequencePlayerEvent OnPlay;

	/** Event triggered when the level sequence player is played in reverse */
	UPROPERTY(BlueprintAssignable, Category = "Sequencer|Player")
	FOnMovieSceneSequencePlayerEvent OnPlayReverse;

	/** Event triggered when the level sequence player is stopped */
	UPROPERTY(BlueprintAssignable, Category = "Sequencer|Player")
	FOnMovieSceneSequencePlayerEvent OnStop;

	/** Event triggered when the level sequence player is paused */
	UPROPERTY(BlueprintAssignable, Category = "Sequencer|Player")
	FOnMovieSceneSequencePlayerEvent OnPause;

	/** Event triggered when the level sequence player finishes naturally (without explicitly calling stop) */
	UPROPERTY(BlueprintAssignable, Category = "Sequencer|Player")
	FOnMovieSceneSequencePlayerEvent OnFinished;


public:

	/** Retrieve all objects currently bound to the specified binding identifier */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	TArray<UObject*> GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding);

	/** Get the object bindings for the requested object */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	TArray<FMovieSceneObjectBindingID> GetObjectBindings(UObject* InObject);

public:

	/** Ensure that this player's tick manager is set up correctly for the specified context */
	void InitializeForTick(UObject* Context);

	/** Assign this player's playback settings */
	void SetPlaybackSettings(const FMovieSceneSequencePlaybackSettings& InSettings);

	/** Initialize this player using its existing playback settings */
	void Initialize(UMovieSceneSequence* InSequence);

	/** Initialize this player with a sequence and some settings */
	void Initialize(UMovieSceneSequence* InSequence, const FMovieSceneSequencePlaybackSettings& InSettings);

	/** Update the sequence for the current time, if playing */
	void Update(const float DeltaSeconds);

	/** Update the sequence for the current time, if playing, asynchronously */
	void UpdateAsync(const float DeltaSeconds);

	/** Perform any tear-down work when this player is no longer (and will never) be needed */
	void TearDown();

public:

	/**
	 * Access the sequence this player is playing
	 * @return the sequence currently assigned to this player
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	UMovieSceneSequence* GetSequence() const { return Sequence; }

	/**
	 * Get the name of the sequence this player is playing
	 * @param bAddClientInfo  If true, add client index if running as a client
	 * @return the name of the sequence, or None if no sequence is set
	 */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Player")
	FString GetSequenceName(bool bAddClientInfo = false) const;

	/**
	 * Access this player's tick manager
	 */
	UMovieSceneSequenceTickManager* GetTickManager() const { return TickManager; }

	/**
	 * Assign a playback client interface for this sequence player, defining instance data and binding overrides
	 */
	void SetPlaybackClient(TScriptInterface<IMovieScenePlaybackClient> InPlaybackClient);

	/**
	 * Assign a time controller for this sequence player allowing custom time management implementations.
	 */
	void SetTimeController(TSharedPtr<FMovieSceneTimeController> InTimeController);

	/**
	 * Sets whether to listen or ignore playback replication events.
	 * @param bState If true, ignores playback replication.
	 */
	void SetIgnorePlaybackReplication(bool bState);

protected:

	void PlayInternal();
	void StopInternal(FFrameTime TimeToResetTo);
	void FinishPlaybackInternal(FFrameTime TimeToFinishAt);

	struct FMovieSceneUpdateArgs
	{
		bool bHasJumped = false;
		bool bIsAsync = false;
	};

	void UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, bool bHasJumped = false);
	virtual void UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, const FMovieSceneUpdateArgs& Args);

	void UpdateTimeCursorPosition(FFrameTime NewPosition, EUpdatePositionMethod Method, bool bHasJumpedOverride = false);
	bool ShouldStopOrLoop(FFrameTime NewPosition) const;
	/** 
	* If the current sequence should pause (due to NewPosition overshooting a previously set ShouldPause) 
	* then a range of time that should be evaluated to reach there will be returned. If we should not pause
	* then the TOptional will be unset.
	* */
	TOptional<TRange<FFrameTime>> GetPauseRange(const FFrameTime& NewPosition) const;

	UWorld* GetPlaybackWorld() const;

	FFrameTime GetLastValidTime() const;

	FFrameRate GetDisplayRate() const;

	bool NeedsQueueLatentAction() const;
	void QueueLatentAction(FMovieSceneSequenceLatentActionDelegate Delegate);
	void RunLatentActions();

public:
	//~ IMovieScenePlayer interface
	virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() override { return RootTemplateInstance; }

protected:
	//~ IMovieScenePlayer interface
	virtual UMovieSceneEntitySystemLinker* ConstructEntitySystemLinker() override;
	virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override;
	virtual FMovieSceneSpawnRegister& GetSpawnRegister() override;
	virtual UObject* AsUObject() override { return this; }

	virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override {}
	virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override {}
	virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override {}
	virtual bool CanUpdateCameraCut() const override { return !PlaybackSettings.bDisableCameraCuts; }
	virtual void UpdateCameraCut(UObject* CameraObject, const EMovieSceneCameraCutParams& CameraCutParams) override {}
	virtual void ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual IMovieScenePlaybackClient* GetPlaybackClient() override { return PlaybackClient ? &*PlaybackClient : nullptr; }
	virtual bool IsDisablingEventTriggers(FFrameTime& DisabledUntilTime) const override;
	virtual void PreEvaluation(const FMovieSceneContext& Context) override;
	virtual void PostEvaluation(const FMovieSceneContext& Context) override;

	/*~ Begin UObject interface */
	virtual bool IsSupportedForNetworking() const { return true; }
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;
	virtual void PostNetReceive() override;
	virtual void BeginDestroy() override;
	/*~ End UObject interface */

	//~ Begin IMovieSceneSequenceTickManagerClient interface
	virtual void TickFromSequenceTickManager(float DeltaSeconds, FMovieSceneEntitySystemRunner* Runner) override;
	//~ End IMovieSceneSequenceTickManagerClient interface

protected:

	virtual bool CanPlay() const { return true; }
	virtual void OnStartedPlaying() {}
	virtual void OnLooped() {}
	virtual void OnPaused() {}
	virtual void OnStopped() {}
	
private:

	void UpdateTimeCursorPosition_Internal(FFrameTime NewPosition, EUpdatePositionMethod Method, bool bHasJumpedOverride);

	void RunPreEvaluationCallbacks();
	void RunPostEvaluationCallbacks();
	
private:

	/**
	 * Called on the server whenever an explicit change in time has occurred through one of the (Play|Jump|Scrub)To methods
	 */
	UFUNCTION(netmulticast, reliable)
	void RPC_ExplicitServerUpdateEvent(EUpdatePositionMethod Method, FFrameTime RelevantTime);

	/**
	 * Called on the server when Stop() is called in order to differentiate Stops from Pauses.
	 */
	UFUNCTION(netmulticast, reliable)
	void RPC_OnStopEvent(FFrameTime StoppedTime);

	/**
	 * Called on the server when playback has reached the end. Could lead to stopping or pausing.
	 */
	UFUNCTION(netmulticast, reliable)
	void RPC_OnFinishPlaybackEvent(FFrameTime StoppedTime);

	/**
	 * Check whether this sequence player is an authority, as determined by its outer Actor
	 */
	bool HasAuthority() const;

	/**
	 * Update the replicated properties required for synchronizing to clients of this sequence player
	 */
	void UpdateNetworkSyncProperties();

	/**
	 * Analyse the set of samples we have estimating the server time if we have confidence over the data.
	 * Should only be called once per frame.
	 * @return An estimation of the server time, or the current local time if we cannot make a strong estimate
	 */
	FFrameTime UpdateServerTimeSamples();

protected:

	/** Movie player status. */
	UPROPERTY()
	TEnumAsByte<EMovieScenePlayerStatus::Type> Status;

	/** Whether we're currently playing in reverse. */
	UPROPERTY(replicated)
	uint32 bReversePlayback : 1;

	/** Set to true to invoke OnStartedPlaying on first update tick for started playing */
	uint32 bPendingOnStartedPlaying : 1;

	/** Set to true while the player's sequence is being evaluated */
	uint32 bIsEvaluating : 1;

	/** Set to true when the player is currently in the main level update */
	uint32 bIsAsyncUpdate : 1;

	/** Flag that allows the player to tick its time controller without actually evaluating the sequence */
	uint32 bSkipNextUpdate : 1;

	/** The sequence to play back */
	UPROPERTY(transient)
	TObjectPtr<UMovieSceneSequence> Sequence;

	/** Time (in playback frames) at which to start playing the sequence (defaults to the lower bound of the sequence's play range) */
	UPROPERTY(replicated)
	FFrameNumber StartTime;

	/** Time (in playback frames) at which to stop playing the sequence (defaults to the upper bound of the sequence's play range) */
	UPROPERTY(replicated)
	int32 DurationFrames;

	UPROPERTY(replicated)
	float DurationSubFrames;

	/** The number of times we have looped in the current playback */
	UPROPERTY(transient)
	int32 CurrentNumLoops;

	/** Specific playback settings for the animation. */
	UPROPERTY(replicated)
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	/** The root template instance we're evaluating */
	UPROPERTY(transient)
	FMovieSceneRootEvaluationTemplateInstance RootTemplateInstance;

	/** Usually nullptr, but will be set when we are updating inside a TickFromSequenceTickManager call */
	FMovieSceneEntitySystemRunner* CurrentRunner;

	/** Play position helper */
	FMovieScenePlaybackPosition PlayPosition;

	/** Disable event triggers until given time */
	TOptional<FFrameTime> DisableEventTriggersUntilTime;

	/** Spawn register */
	TSharedPtr<FMovieSceneSpawnRegister> SpawnRegister;

	struct FServerTimeSample
	{
		/** The actual server sequence time in seconds, with client ping at the time of the sample baked in */
		double ServerTime;
		/** Wall-clock time that the sample was receieved */
		double ReceievedTime;
	};
	/**
	 * Array of server sequence times in seconds, with ping compensation baked in.
	 * Samples are sorted chronologically with the oldest samples first
	 */
	TArray<FServerTimeSample> ServerTimeSamples;

	/** Replicated playback status and current time that are replicated to clients */
	UPROPERTY(replicated)
	FMovieSceneSequenceReplProperties NetSyncProps;

	/** External client pointer in charge of playing back this sequence */
	UPROPERTY(Transient)
	TScriptInterface<IMovieScenePlaybackClient> PlaybackClient;

	/** Global tick manager, held here to keep it alive while world sequences are in play */
	UPROPERTY(transient)
	TObjectPtr<UMovieSceneSequenceTickManager> TickManager;

	/** Local latent action manager for when we're running a blocking sequence */
	FMovieSceneLatentActionManager LatentActionManager;

	/** (Optional) Externally supplied time controller */
	TSharedPtr<FMovieSceneTimeController> TimeController;

	/** (Optional) Synchronous runner to use when no tick manager is in use */
	TSharedPtr<FMovieSceneEntitySystemRunner> SynchronousRunner;

	/** When true, ignore playback replication events. */
	bool bIgnorePlaybackReplication = false;

private:

	/** The event that will be broadcast every time the sequence is updated */
	mutable FOnMovieSceneSequencePlayerUpdated OnMovieSceneSequencePlayerUpdate;

	/** The tick interval we are currently registered with (if any) */
	TOptional<FMovieSceneSequenceTickInterval> RegisteredTickInterval;

	/** The maximum tick rate prior to playing (used for overriding delta time during playback). */
	TOptional<double> OldMaxTickRate;

	/**
	* The last world game time at which we were ticked. Game time used is dependent on bTickEvenWhenPaused
	* Valid only if we've been ticked at least once since having a tick interval
	*/
	TOptional<float> LastTickGameTimeSeconds;

	struct FPauseOnArgs
	{
		FFrameTime Time;
		bool bExclusive;
	};

	/** If set, pause playback on this frame */
	TOptional<FPauseOnArgs> PauseOnFrame;

	/** Pre and post evaluation callbacks, for async evaluations */
	DECLARE_DELEGATE(FOnEvaluationCallback);
	TArray<FOnEvaluationCallback> PreEvaluationCallbacks;
	TArray<FOnEvaluationCallback> PostEvaluationCallbacks;
};
