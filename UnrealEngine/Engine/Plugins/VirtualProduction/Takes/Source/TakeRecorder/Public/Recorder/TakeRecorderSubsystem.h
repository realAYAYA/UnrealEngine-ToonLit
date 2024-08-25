// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "TakeRecorderSubsystem.generated.h"

class ULevelSequence;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTakeRecorderPreInitialize);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTakeRecorderStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTakeRecorderStopped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTakeRecorderFinished, ULevelSequence*, SequenceAsset);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTakeRecorderCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTakeRecorderMarkedFrameAdded, const FMovieSceneMarkedFrame&, MarkedFrame);

/**
* UTakeRecorderSubsystem
* Subsystem for Take Recorder
*/
UCLASS()
class TAKERECORDER_API UTakeRecorderSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:

	/** Called before initialization occurs (ie. when the recording button is pressed and before the countdown starts) */
	UPROPERTY(BlueprintAssignable, Category = "Take Recorder")
	FTakeRecorderPreInitialize TakeRecorderPreInitialize;

	/** Called when take recorder is started */
	UPROPERTY(BlueprintAssignable, Category = "Take Recorder")
	FTakeRecorderStarted TakeRecorderStarted;

	/** Called when take recorder is stopped */
	UPROPERTY(BlueprintAssignable, Category = "Take Recorder")
	FTakeRecorderStopped TakeRecorderStopped;

	/** Called when take recorder has finished */
	UPROPERTY(BlueprintAssignable, Category = "Take Recorder")
	FTakeRecorderFinished TakeRecorderFinished;

	/** Called when take recorder is cancelled */
	UPROPERTY(BlueprintAssignable, Category = "Take Recorder")
	FTakeRecorderCancelled TakeRecorderCancelled;

	/** Called when a marked frame is added to take recorder */
	UPROPERTY(BlueprintAssignable, Category = "Take Recorder")
	FTakeRecorderMarkedFrameAdded TakeRecorderMarkedFrameAdded;
};