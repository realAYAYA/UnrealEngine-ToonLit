// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "ISequencer.h"
#include "Recorder/TakeRecorderParameters.h"
#include "Serializers/MovieSceneManifestSerialization.h"
#include "TakeRecorder.generated.h"

class ISequencer;
class UTakePreset;
class UTakeMetaData;
class UTakeRecorder;
class ULevelSequence;
class SNotificationItem;
class UTakeRecorderSources;
class UTakeRecorderOverlayWidget;
class UMovieSceneSequence;


UENUM(BlueprintType)
enum class ETakeRecorderState : uint8
{
	CountingDown,
	PreRecord,
	TickingAfterPre,
	Started,
	Stopped,
	Cancelled,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTakeRecordingPreInitialize, UTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTakeRecordingInitialized, UTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTakeRecordingStarted, UTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTakeRecordingStopped, UTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTakeRecordingFinished, UTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTakeRecordingCancelled, UTakeRecorder*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStartPlayFrameModified, UTakeRecorder*, const FFrameNumber& StartFrame);

DECLARE_DELEGATE_RetVal_OneParam(FTakeRecorderParameters, FTakeRecorderParameterDelegate,const FTakeRecorderParameters&);

struct TAKERECORDER_API FTakeRecorderParameterOverride
{
	void RegisterHandler(FName OverrideName, FTakeRecorderParameterDelegate Delegate);
	void UnregisterHandler(FName OverrideName);

	TMap<FName, FTakeRecorderParameterDelegate> Delegates;
};

DECLARE_LOG_CATEGORY_EXTERN(ManifestSerialization, Verbose, All);

UCLASS(BlueprintType)
class TAKERECORDER_API UTakeRecorder : public UObject
{
public:

	GENERATED_BODY()

	UTakeRecorder(const FObjectInitializer& ObjInit);

public:

	/**
	 * Retrieve the currently active take recorder instance
	 */
	static UTakeRecorder* GetActiveRecorder();

	/**
	 * Retrieve a multi-cast delegate that is triggered when a new recording begins
	 */
	static FOnTakeRecordingInitialized& OnRecordingInitialized();

	/**
	 * On take initialization overrides provide a mechanism to adjust parameter values when
	 * a new take is initialized.
	 */
	static FTakeRecorderParameterOverride& TakeInitializeParameterOverride();

public:

	/**
	 * Access the number of seconds remaining before this recording will start
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	float GetCountdownSeconds() const
	{
		return CountdownSeconds;
	}

	/**
	 * Access the sequence asset that this recorder is recording into
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	ULevelSequence* GetSequence() const
	{
		return SequenceAsset;
	}

	/**
	 * Get the current state of this recorder
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	ETakeRecorderState GetState() const
	{
		return State;
	}

	/**
	 * Disable saving data on tick event. This make the TakeRecord a no-op.
	 */
	void SetDisableSaveTick( bool );

public:

	/**
	 * Initialize a new recording with the specified parameters. Fails if another recording is currently in progress.
	 *
	 * @param LevelSequenceBase   A level sequence to use as a base set-up for the take
	 * @param Sources             The sources to record from
	 * @param MetaData            Meta data to store on the recorded level sequence asset for this take
	 * @param Parameters          Configurable parameters for this instance of the recorder
	 * @param OutError            Error string to receive for context
	 * @return True if the recording process was successfully initialized, false otherwise
	 */
	bool Initialize(ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, const FTakeRecorderParameters& InParameters, FText* OutError = nullptr);

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
	FOnTakeRecordingPreInitialize& OnRecordingPreInitialize();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording starts
	 */
	FOnTakeRecordingStarted& OnRecordingStarted();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording is stopped
	 */
	FOnTakeRecordingStopped& OnRecordingStopped();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording finishes
	 */
	FOnTakeRecordingFinished& OnRecordingFinished();

	/**
	 * Retrieve a multi-cast delegate that is triggered when this recording is cancelled
	 */
	FOnTakeRecordingCancelled& OnRecordingCancelled();

	/**
	 * Retrieve a multi-cast delegate that is triggered when a delta time has been applied to the Movie Scene
	 */
	FOnStartPlayFrameModified& OnStartPlayFrameModified();

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
	 * Create a new destination asset to record into based on the parameters
	 */
	bool CreateDestinationAsset(const TCHAR* AssetPathFormat, ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, FText* OutError);

	/**
	 * Setup a existing asset to record into based on the parameters
	 */
	bool SetupDestinationAsset(const FTakeRecorderParameters& InParameters, ULevelSequence* LevelSequenceBase, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, FText* OutError);

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
	 * It takes into account TakeRecorder project settings, the command line, and global unattended settings.
     */
	bool ShouldShowNotifications();

private:

	/* The time at which to record. Taken from the Sequencer global time, otherwise based on timecode */
	FQualifiedFrameTime GetRecordTime() const;

	/** Called by Tick and Start to make sure we record at start */
	void InternalTick(float DeltaTime);

	virtual UWorld* GetWorld() const override;

private:

	/** The number of seconds remaining before Start() should be called */
	float CountdownSeconds;

	/** The state of this recorder instance */
	ETakeRecorderState State;

	/** FFrameTime in MovieScene Resolution we are at*/
	FFrameTime CurrentFrameTime;

	/** Timecode at the start of recording */
	FTimecode TimecodeAtStart;

	/** Optional frame to stop recording at*/
	TOptional<FFrameNumber> StopRecordingFrame;

	/** The asset that we should output recorded data into */
	UPROPERTY(transient)
	TObjectPtr<ULevelSequence> SequenceAsset;

	/** The overlay widget for this recording */
	UPROPERTY(transient)
	TObjectPtr<UTakeRecorderOverlayWidget> OverlayWidget;

	/** The world that we are recording within */
	UPROPERTY(transient)
	TWeakObjectPtr<UWorld> WeakWorld;

	/** Parameters for the recorder - marked up as a uproperty to support reference collection */
	UPROPERTY()
	FTakeRecorderParameters Parameters;

	/** Anonymous array of cleanup functions to perform when a recording has finished */
	TArray<TFunction<void()>> OnStopCleanup;

	/** Triggered before the recorder is initialized */
	FOnTakeRecordingPreInitialize OnRecordingPreInitializeEvent;

	/** Triggered when this recorder starts */
	FOnTakeRecordingStarted OnRecordingStartedEvent;

	/** Triggered when this recorder is stopped */
	FOnTakeRecordingStopped OnRecordingStoppedEvent;

	/** Triggered when this recorder finishes */
	FOnTakeRecordingFinished OnRecordingFinishedEvent;

	/** Triggered when this recorder is cancelled */
	FOnTakeRecordingCancelled OnRecordingCancelledEvent;

	/** Triggered when the movie scene is adjusted. */
	FOnStartPlayFrameModified OnFrameModifiedEvent;

	/** Sequencer ptr that controls playback of the desination asset during the recording */
	TWeakPtr<ISequencer> WeakSequencer;

	/** Due a few ticks after the pre so we are set up with asset creation */
	int32 NumberOfTicksAfterPre;

	friend class FTickableTakeRecorder;

private:

	/**
	 * Set the currently active take recorder instance
	 */
	static bool SetActiveRecorder(UTakeRecorder* NewActiveRecorder);

	/** Event to trigger when a new recording is initialized */
	static FOnTakeRecordingInitialized OnRecordingInitializedEvent;

private:

	FManifestSerializer ManifestSerializer;

	EAllowEditsMode CachedAllowEditsMode;
	EAutoChangeMode CachedAutoChangeMode;
	EUpdateClockSource CachedClockSource;
	
	TRange<FFrameNumber> CachedPlaybackRange;
};
