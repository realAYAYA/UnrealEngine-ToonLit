// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Misc/QualifiedFrameTime.h"
#include "Math/Range.h"
#include "ConcertSequencerMessages.generated.h"


/**
 * Enum for the current Sequencer player status, should match EMovieScenePlayerStatus::Type
 * Defined here to not have a dependency on the MovieScene module.
 */
UENUM()
enum class EConcertMovieScenePlayerStatus : uint8
{
	Stopped,
	Playing,
	Scrubbing,
	Jumping,
	Stepping,
	Paused,
	MAX
};

USTRUCT()
struct FConcertSequencerState
{
	GENERATED_BODY()

	/** The full path name to the root sequence that is open on the sequencer */
	UPROPERTY()
	FString SequenceObjectPath;

	/** The time that the sequence is at */
	UPROPERTY()
	FQualifiedFrameTime Time;

	/** The current status of the sequencer player */
	UPROPERTY()
	EConcertMovieScenePlayerStatus PlayerStatus;

	UPROPERTY()
	FFrameNumberRange PlaybackRange;

	/** The current playback speed */
	UPROPERTY()
	float PlaybackSpeed;

	UPROPERTY()
	bool bLoopMode = false;

	FConcertSequencerState()
		: PlayerStatus(EConcertMovieScenePlayerStatus::Stopped)
		, PlaybackSpeed(1.0f)
	{}
};

/**
 * Event that signals a Sequencer just been opened.
 */
USTRUCT()
struct FConcertSequencerOpenEvent
{
	GENERATED_BODY()

	/** The full path name to the root sequence of the sequencer that just opened. */
	UPROPERTY()
	FString SequenceObjectPath;
};

/**
 * Event that signals a Sequencer just been closed.
 */
USTRUCT()
struct FConcertSequencerCloseEvent
{
	GENERATED_BODY()

	/** The full path name to the root sequence of the sequencer that just closed. */
	UPROPERTY()
	FString SequenceObjectPath;

	UPROPERTY()
	bool bControllerClose = false;

	UPROPERTY()
	int32 EditorsWithSequencerOpened = -1;
};

/**
 * Event that signals a sequencer UI has changed the current state
 */
USTRUCT()
struct FConcertSequencerStateEvent
{
	GENERATED_BODY()

	/** The new state that the sequence is at */
	UPROPERTY()
	FConcertSequencerState State;
};

/**
 * Event that represent the current open sequencer states to a newly connected client
 */
USTRUCT()
struct FConcertSequencerStateSyncEvent
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FConcertSequencerState> SequencerStates;
};

/**
 * An event that represents a time changes on the sequencer. This can happen via take recorder
 * which will shift the active take _if_ Start At Timecode is enabled.
 */
USTRUCT()
struct FConcertSequencerTimeAdjustmentEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FFrameNumber PlaybackStartFrame;

	UPROPERTY()
	FString SequenceObjectPath;
};
