// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "MovieSceneFwd.h"
#include "TrackRecorders/IMovieSceneTrackRecorderHost.h"
#include "TakeRecorderParameters.generated.h"

USTRUCT(BlueprintType)
struct FTakeRecorderUserParameters
{
	GENERATED_BODY()

	TAKERECORDER_API FTakeRecorderUserParameters();

	/** Whether to maximize the viewport (enter Immersive Mode) when recording */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category="User Settings")
	bool bMaximizeViewport;

	/** Delay that we will use before starting recording */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category="User Settings", DisplayName="Countdown", meta=(Units=s, ClampMin="0.0", UIMin="0.0", ClampMax="60.0", UIMax="60.0"))
	float CountdownSeconds;

	/** The engine time dilation to apply during the recording */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category="User Settings", meta=(Units=Multiplier, ClampMin="0.00001", UIMin="0.00001"))
	float EngineTimeDilation;

	/** Reset playhead to beginning of the playback range when starting recording */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "User Settings")
	bool bResetPlayhead;

	/** Automatically stop recording when reaching the end of the playback range */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "User Settings")
	bool bStopAtPlaybackEnd;

	/** Recommended for use with recorded spawnables. Beware that changes to actor instances in the map after recording may alter the recording when played back */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category="User Settings")
	bool bRemoveRedundantTracks;

	/** Tolerance to use when reducing keys */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category="User Settings")
	float ReduceKeysTolerance;

	/** Whether to save recorded level sequences and assets when done recording */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "User Settings")
	bool bSaveRecordedAssets;

	/** Whether to lock the level sequence when done recording */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "User Settings")
	bool bAutoLock;

	/** Whether to incrementally serialize and store some data while recording*/
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "User Settings")
	bool bAutoSerialize;
};

USTRUCT(BlueprintType)
struct FTakeRecorderProjectParameters
{
	GENERATED_BODY()

	TAKERECORDER_API FTakeRecorderProjectParameters();

	/** The take asset path, composed of the TakeRootSaveDir and the TakeSaveDir */
	TAKERECORDER_API FString GetTakeAssetPath() const { return RootTakeSaveDir.Path / TakeSaveDir; }

	/** The root of the directory in which to save recorded takes. */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder", meta = (ContentDir))
	FDirectoryPath RootTakeSaveDir;

	/**
	 * The name of the directory in which to save recorded takes. Supports any of the following format specifiers that will be substituted when a take is recorded:
	 * {day}       - The day of the timestamp for the start of the recording.
	 * {month}     - The month of the timestamp for the start of the recording.
	 * {year}      - The year of the timestamp for the start of the recording.
	 * {hour}      - The hour of the timestamp for the start of the recording.
	 * {minute}    - The minute of the timestamp for the start of the recording.
	 * {second}    - The second of the timestamp for the start of the recording.
	 * {take}      - The take number.
	 * {slate}     - The slate string.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	FString TakeSaveDir;

	/**
	 * The default name to use for the Slate information
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	FString DefaultSlate;

	/**
	 * The clock source to use when recording
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	EUpdateClockSource RecordingClockSource;

	/**
	 * If enabled, track sections will start at the current timecode. Otherwise, 0.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	bool bStartAtCurrentTimecode;

	/**
	 * If enabled, timecode will be recorded into each actor track
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	bool bRecordTimecode;

	/**
	* If enabled, each Source will be recorded into a separate Sequence and embedded in the Master Sequence will link to them via Subscenes track.
	* If disabled, all Sources will be recorded into the Master Sequence, and you will not be able to swap between various takes of specific source
	* using the Sequencer Take ui. This can still be done via copying and pasting between sequences if needed.
	*/
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	bool bRecordSourcesIntoSubSequences;

	/*
	 * If enabled, all recorded actors will be recorded to possessable object bindings in Sequencer. If disabled, all recorded actors will be 
	 * recorded to spawnable object bindings in Sequencer. This can be overridden per actor source.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	bool bRecordToPossessable;

	/** List of property names for which movie scene tracks will always record. */
	UPROPERTY(config, EditAnywhere, Category = "Take Recorder")
	TArray<FTakeRecorderTrackSettings> DefaultTracks;

	/** Whether to show notification windows or not when recording */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	bool bShowNotifications;
};

UENUM(BlueprintType)
enum class ETakeRecorderMode : uint8
{
	/* Record into a new sequence */
	RecordNewSequence,

	/* Record into an existing sequence */
	RecordIntoSequence
};

/**
 * Structure housing all configurable parameters for a take recorder instance
 */
USTRUCT(BlueprintType)
struct FTakeRecorderParameters
{
	GENERATED_BODY()

	TAKERECORDER_API FTakeRecorderParameters();

	UPROPERTY(BlueprintReadWrite, Category="Take Recorder")
	FTakeRecorderUserParameters User;

	UPROPERTY(BlueprintReadWrite, Category="Take Recorder")
	FTakeRecorderProjectParameters Project;

	UPROPERTY(BlueprintReadWrite, Category="Take Recorder")
	ETakeRecorderMode TakeRecorderMode;

	UPROPERTY(BlueprintReadWrite, Category = "Take Recorder")
	FFrameNumber StartFrame;

	/**
	 * Option to disable recording and saving of data. This can be used in a scenario where multiple clients are running
	 * take recorder, but only certain ones are set to process and save the data.
	 */
	UPROPERTY(Transient)
	bool bDisableRecordingAndSave = false;
};
