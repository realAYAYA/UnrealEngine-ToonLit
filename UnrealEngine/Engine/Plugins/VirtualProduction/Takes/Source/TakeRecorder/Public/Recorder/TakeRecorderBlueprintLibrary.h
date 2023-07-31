// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Recorder/TakeRecorder.h"
#include "TakeRecorderBlueprintLibrary.generated.h"

class UTakeRecorder;
class UTakeMetaData;
class ULevelSequence;
class UTakeRecorderPanel;

DECLARE_DYNAMIC_DELEGATE( FOnTakeRecorderPanelChanged );
DECLARE_DYNAMIC_DELEGATE( FOnTakeRecorderPreInitialize );
DECLARE_DYNAMIC_DELEGATE( FOnTakeRecorderStarted );
DECLARE_DYNAMIC_DELEGATE( FOnTakeRecorderStopped );
DECLARE_DYNAMIC_DELEGATE_OneParam( FOnTakeRecorderFinished, ULevelSequence*, SequenceAsset );
DECLARE_DYNAMIC_DELEGATE( FOnTakeRecorderCancelled );
DECLARE_DYNAMIC_DELEGATE_OneParam( FOnTakeRecorderMarkedFrameAdded, const FMovieSceneMarkedFrame&, MarkedFrame );

UCLASS()
class TAKERECORDER_API UTakeRecorderBlueprintLibrary : public UBlueprintFunctionLibrary
{
public:

	GENERATED_BODY()


	/**
	 * Is the Take Recorder enabled in the build
	 */
	UFUNCTION(BlueprintPure, Category="Take Recorder")
	static bool IsTakeRecorderEnabled();

	/**
	 * Start a new recording using the specified parameters. Will fail if a recording is currently in progress
	 *
	 * @param LevelSequence         The base level sequence to use for the recording. Will be played back during the recording and duplicated to create the starting point for the resulting asset.
	 * @param Sources               The sources to use for the recording
	 * @param MetaData              Meta-data pertaining to this recording, duplicated into the resulting recorded sequence
	 * @param Parameters            Configurable parameters for this recorder instance
	 * @return The recorder responsible for the recording, or None if a a recording could not be started
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static UTakeRecorder* StartRecording(ULevelSequence* LevelSequence, UTakeRecorderSources* Sources, UTakeMetaData* MetaData, const FTakeRecorderParameters& Parameters);


	/**
	 * Get the default recorder parameters according to the project and user settings
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static FTakeRecorderParameters GetDefaultParameters();


	/**
	 * Set the default recorder parameters
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static void SetDefaultParameters(const FTakeRecorderParameters& DefaultParameters);

	/**
	 * Check whether a recording is currently active
	 */
	UFUNCTION(BlueprintPure, Category="Take Recorder")
	static bool IsRecording();


	/**
	 * Retrieve the currently active recorder, or None if there none are active
	 */
	UFUNCTION(BlueprintPure, Category="Take Recorder")
	static UTakeRecorder* GetActiveRecorder();


	/**
	 * Stop recording if there is a recorder currently active
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static void StopRecording();


	/**
	 * Cancel recording if there is a recorder currently active
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static void CancelRecording();


	/**
	 * Get the currently open take recorder panel, if one is open
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Take Recorder")
	static UTakeRecorderPanel* GetTakeRecorderPanel();


	/**
	 * Get the currently open take recorder panel, if one is open, opening a new one if not
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder")
	static UTakeRecorderPanel* OpenTakeRecorderPanel();


	/** Called when a Take Panel is constructed or destroyed. */
	UFUNCTION(BlueprintCallable, Category="Take Recorder", meta=(DisplayName="Set On Take Recorder Panel Changed"))
	static void SetOnTakeRecorderPanelChanged(FOnTakeRecorderPanelChanged OnTakeRecorderPanelChanged);

	/** Called before initialization occurs (ie. when the recording button is pressed and before the countdown starts) */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static void SetOnTakeRecorderPreInitialize(FOnTakeRecorderPreInitialize OnTakeRecorderPreInitialize);

	/** Called when take recording starts. */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static void SetOnTakeRecorderStarted(FOnTakeRecorderStarted OnTakeRecorderStarted);

	/** Called when take recording is stopped. */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static void SetOnTakeRecorderStopped(FOnTakeRecorderStopped OnTakeRecorderStopped);

	/** Called when take recording finishes. */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static void SetOnTakeRecorderFinished(FOnTakeRecorderFinished OnTakeRecorderFinished);

	/** Called when take recording is cancelled. */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static void SetOnTakeRecorderCancelled(FOnTakeRecorderCancelled OnTakeRecorderCancelled);

	/** Called when a marked frame is added. */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder")
	static void SetOnTakeRecorderMarkedFrameAdded(FOnTakeRecorderMarkedFrameAdded OnTakeRecorderMarkedFrameAdded);

	static void OnTakeRecorderPreInitialize();
	static void OnTakeRecorderStarted();
	static void OnTakeRecorderStopped();
	static void OnTakeRecorderFinished(ULevelSequence* InSequenceAsset);
	static void OnTakeRecorderCancelled();
	static void OnTakeRecorderMarkedFrameAdded(const FMovieSceneMarkedFrame& InMarkedFrame);

	/**
	 * Internal function to assign a new take recorder panel singleton.
	 * NOTE: Only to be called by STakeRecorderTabContent::Construct.
	 */
	static void SetTakeRecorderPanel(UTakeRecorderPanel* InNewPanel);
};
