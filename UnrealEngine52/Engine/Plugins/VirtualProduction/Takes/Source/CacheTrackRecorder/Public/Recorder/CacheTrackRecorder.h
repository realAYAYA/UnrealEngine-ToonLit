// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "ISequencer.h"
#include "CacheTrackRecorder.generated.h"

class ULevelSequence;
class UMovieSceneTrackRecorder;
class IMovieSceneCachedTrack;
class UTakeMetaData;

USTRUCT()
struct CACHETRACKRECORDER_API FCacheRecorderUserParameters
{
	GENERATED_BODY()

	FCacheRecorderUserParameters();

	/** Whether to maximize the viewport (enter Immersive Mode) when recording */
	UPROPERTY(config, EditAnywhere, Category="User Settings")
	bool bMaximizeViewport;

	/** Delay that we will use before starting recording */
	UPROPERTY(config, EditAnywhere, Category="User Settings", DisplayName="Countdown", meta=(Units=s, ClampMin="0.0", UIMin="0.0", ClampMax="60.0", UIMax="60.0"))
	float CountdownSeconds;

	/** The engine time dilation to apply during the recording */
	UPROPERTY(config, EditAnywhere, Category="User Settings", meta=(Units=Multiplier, ClampMin="0.00001", UIMin="0.00001"))
	float EngineTimeDilation;

	/** Reset playhead to beginning of the playback range when starting recording */
	UPROPERTY(config, EditAnywhere, Category = "User Settings")
	bool bResetPlayhead;

	/** Automatically stop recording when reaching the end of the playback range */
	UPROPERTY(config, EditAnywhere, Category = "User Settings")
	bool bStopAtPlaybackEnd;
};

USTRUCT()
struct CACHETRACKRECORDER_API FCacheRecorderProjectParameters
{
	GENERATED_BODY()

	FCacheRecorderProjectParameters();
	
	/**
	 * The default name to use for the Slate information
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cache Recorder")
	FString DefaultSlate;

	/**
	 * If true then take recorder will control the sequencer timing when recording with a fixed editor time step. The delta time is derived by the sequence's target frame rate. 
	 * This is useful when recording cache data where frame accuracy is important (e.g. Niagara systems), but should be set to false when dealing with data from external sources (e.g. LiveLink).
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cache Recorder")
	bool bCacheTrackRecorderControlsClockTime = true;

	/**
	 * The clock source to use when recording
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cache Recorder", meta=(EditCondition="!bCacheTrackRecorderControlsClockTime"))
	EUpdateClockSource RecordingClockSource;

	/**
	 * If enabled, track sections will start at the current timecode. Otherwise, 0.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cache Recorder")
	bool bStartAtCurrentTimecode;

	/**
	 * If enabled, timecode will be recorded into each actor track
	 */
	UPROPERTY(config, EditAnywhere, Category = "Cache Recorder")
	bool bRecordTimecode;

	/** Whether to show notification windows or not when recording */
	UPROPERTY(config, EditAnywhere, Category = "Cache Recorder")
	bool bShowNotifications;
};

/**
 * Structure housing all configurable parameters for a take recorder instance
 */
USTRUCT()
struct CACHETRACKRECORDER_API FCacheRecorderParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Cache Recorder")
	FCacheRecorderUserParameters User;

	UPROPERTY(EditAnywhere, Category="Cache Recorder")
	FCacheRecorderProjectParameters Project;

	UPROPERTY(EditAnywhere, Category = "Cache Recorder")
	FFrameNumber StartFrame;
};

UENUM(BlueprintType)
enum class ECacheTrackRecorderState : uint8
{
	CountingDown,
	PreRecord,
	TickingAfterPre,
	Started,
	Stopped,
	Cancelled,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCacheTrackRecordingPreInitialize, UCacheTrackRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCacheTrackRecordingInitialized, UCacheTrackRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCacheTrackRecordingStarted, UCacheTrackRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCacheTrackRecordingStopped, UCacheTrackRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCacheTrackRecordingFinished, UCacheTrackRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCacheTrackRecordingCancelled, UCacheTrackRecorder*);

USTRUCT()
struct FCachedTrackSource
{
	GENERATED_USTRUCT_BODY()
	
	IMovieSceneCachedTrack* Track = nullptr;

	UPROPERTY()
	TObjectPtr<UMovieSceneTrackRecorder> Recorder;
};

UCLASS(BlueprintType)
class CACHETRACKRECORDER_API UCacheTrackRecorder : public UObject
{
public:

	GENERATED_BODY()

	UCacheTrackRecorder(const FObjectInitializer& ObjInit);

public:

	/**
	 * Retrieve the currently active take recorder instance
	 */
	static UCacheTrackRecorder* GetActiveRecorder();

	/**
	 * Retrieve a multi-cast delegate that is triggered when a new recording begins
	 */
	static FOnCacheTrackRecordingInitialized& OnRecordingInitialized();

	/**
	 * Utility method to quickly record a single cache track
	 */
	static void RecordCacheTrack(IMovieSceneCachedTrack* Track, TSharedPtr<ISequencer> Sequencer, FCacheRecorderParameters Parameters = FCacheRecorderParameters());
public:

	/**
	 * Access the number of seconds remaining before this recording will start
	 */
	UFUNCTION(BlueprintCallable, Category="Cache Recorder")
	float GetCountdownSeconds() const
	{
		return CountdownSeconds;
	}

	/**
	 * Access the sequence asset that this recorder is recording into
	 */
	UFUNCTION(BlueprintCallable, Category="Cache Recorder")
	ULevelSequence* GetSequence() const
	{
		return SequenceAsset;
	}

	/**
	 * Get the current state of this recorder
	 */
	UFUNCTION(BlueprintCallable, Category="Cache Recorder")
	ECacheTrackRecorderState GetState() const
	{
		return State;
	}

	/**
	 * Initialize a new recording with the specified parameters. Fails if another recording is currently in progress.
	 *
	 */
	bool Initialize(ULevelSequence* LevelSequenceBase, const TArray<IMovieSceneCachedTrack*>& CacheTracks, const UTakeMetaData* MetaData, const FCacheRecorderParameters& InParameters, FText* OutError = nullptr);

	/**
	 * Called to stop the recording
	 */
	void Stop();

	/**
	 * Called to cancel the recording
	 */
	void Cancel();

	/**
	 * Retrieve a multi-cast delegate that is triggered before initialization occurs (ie. when the recording button is pressed and before the countdown starts)
	 */
	FOnCacheTrackRecordingPreInitialize& OnRecordingPreInitialize();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording starts
	 */
	FOnCacheTrackRecordingStarted& OnRecordingStarted();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording is stopped
	 */
	FOnCacheTrackRecordingStopped& OnRecordingStopped();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording finishes
	 */
	FOnCacheTrackRecordingFinished& OnRecordingFinished();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording is cancelled
	 */
	FOnCacheTrackRecordingCancelled& OnRecordingCancelled();

private:

	/**
	 * Called after the countdown to PreRecord
	 */
	void PreRecord();

	/**
	 * Called after PreRecord To Start
	 */
	void Start();

	/*
	 * Stop or cancel
	 */
	void StopInternal(const bool bCancelled);

	/**
	 * Ticked by a tickable game object to performe any necessary time-sliced logic
	 */
	void Tick(float DeltaTime);

	/**
	 * Called if we're currently recording a PIE world that has been shut down or if we start PIE in a non-PIE world. Bound in Initialize, and unbound in Stop.
	 */
	void HandlePIE(bool bIsSimulating);

	/**
	 * Attempt to open the sequencer UI for the asset to be recorded
	 */
	bool InitializeSequencer(ULevelSequence* LevelSequence, FText* OutError);

	/**
	 * Discovers the source world to record from, and initializes it for recording
	 */
	void DiscoverSourceWorld();

	/**
	 * Called to perform any initialization based on the user-provided parameters
	 */
	void InitializeFromParameters();

	/**
     * Returns true if notification widgets should be shown when recording.
	 * It takes into account CacheTrackRecorder project settings, the command line, and global unattended settings.
     */
	bool ShouldShowNotifications();

	/* Called before setting the editor into a fixed time step mode */
	void BackupEditorTickState();

	/* Sets the editor up to tick in a fixed time step */
	void ModifyEditorTickState();

	/* Restores the editor tick state if necessary */
	void RestoreEditorTickState();

	/* The time at which to record. Taken from the Sequencer global time, otherwise based on timecode */
	FQualifiedFrameTime GetRecordTime() const;

	/** Called by Tick and Start to make sure we record at start */
	void InternalTick(float DeltaTime);

	virtual UWorld* GetWorld() const override;

private:

	/** The number of seconds remaining before Start() should be called */
	float CountdownSeconds;

	/** The state of this recorder instance */
	ECacheTrackRecorderState State;

	/** FFrameTime in MovieScene Resolution we are at*/
	FFrameTime CurrentFrameTime;

	/** Timecode at the start of recording */
	FTimecode TimecodeAtStart;

	/** Optional frame to stop recording at*/
	TOptional<FFrameNumber> StopRecordingFrame;

	/** The asset that we should output recorded data into */
	UPROPERTY(transient)
	TObjectPtr<ULevelSequence> SequenceAsset;

	/** The world that we are recording within */
	UPROPERTY(transient)
	TWeakObjectPtr<UWorld> WeakWorld;

	/** Parameters for the recorder - marked up as a uproperty to support reference collection */
	UPROPERTY()
	FCacheRecorderParameters Parameters;

	UPROPERTY()
	TArray<FCachedTrackSource> CacheTracks;

	/** Anonymous array of cleanup functions to perform when a recording has finished */
	TArray<TFunction<void()>> OnStopCleanup;

	/** Triggered before the recorder is initialized */
	FOnCacheTrackRecordingPreInitialize OnRecordingPreInitializeEvent;

	/** Triggered when this recorder starts */
	FOnCacheTrackRecordingStarted OnRecordingStartedEvent;

	/** Triggered when this recorder is stopped */
	FOnCacheTrackRecordingStopped OnRecordingStoppedEvent;

	/** Triggered when this recorder finishes */
	FOnCacheTrackRecordingFinished OnRecordingFinishedEvent;

	/** Triggered when this recorder is cancelled */
	FOnCacheTrackRecordingCancelled OnRecordingCancelledEvent;

	/** Sequencer ptr that controls playback of the desination asset during the recording */
	TWeakPtr<ISequencer> WeakSequencer;

	/** Due a few ticks after the pre so we are set up with asset creation */
	int32 NumberOfTicksAfterPre;
	
	TRange<double> CachedViewRange;

	friend class FTickableCacheTrackRecorder;

	/**
	 * Set the currently active take recorder instance
	 */
	static bool SetActiveRecorder(UCacheTrackRecorder* NewActiveRecorder);

	/** Event to trigger when a new recording is initialized */
	static FOnCacheTrackRecordingInitialized OnRecordingInitializedEvent;

	EAllowEditsMode CachedAllowEditsMode;
	EAutoChangeMode CachedAutoChangeMode;
	EUpdateClockSource CachedClockSource;
	
	TRange<FFrameNumber> CachedPlaybackRange;

	struct FSavedState
	{
		bool bBackedUp = false;

		bool bUseFixedTimeStep = false;
		double FixedDeltaTime = 1.0;
	};

	FSavedState SavedState;
};
