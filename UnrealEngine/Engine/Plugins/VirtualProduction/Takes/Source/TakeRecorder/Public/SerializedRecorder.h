// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISerializedRecorder.h"
#include "LevelSequence.h"

class UWorld;
class AActor;
struct FActorFileHeader;
struct FActorProperty;
//Implementation of ISerializedRecorder that's defined in the SerializedRecorderInterface Module

class TAKERECORDER_API FSerializedRecorder:  public ISerializedRecorder
{
public:
	FSerializedRecorder() : bLoadSequenceFile(false) {}
	~FSerializedRecorder() {}

	virtual bool LoadRecordedSequencerFile(UMovieSceneSequence* InMovieSceneSequence, UWorld *PlaybackContext, const FString& InFileName, TFunction<void()> InCompletionCallback) override;

	//this is only valid during LoadSequenceFile or LoadActorFile
	AActor GetActorFromGuid(const FGuid& InGuid);

private:
	bool LoadSequenceFile(UMovieSceneSequence* InMovieSceneSequence, UWorld *PlaybackContext, const FString& InFileName, TFunction<void()> InCompletionCallback);
	bool LoadActorFile(UMovieSceneSequence* InMovieSceneSequence, UWorld *PlaybackContext, const FString& InFileName, TFunction<void()> InCompletionCallback);
	bool LoadPropertyFile(UMovieSceneSequence* InMovieSceneSequence, UWorld *PlaybackContext, const FString& InFileName, TFunction<void()> InCompletionCallback);
	bool LoadSubSequenceFile(UMovieSceneSequence* InMovieSceneSequence, UWorld *PlaybackContext, const FString& InFileName, TFunction<void()> InCompletionCallback);

	AActor* SetActorPossesableOrSpawnable(UMovieSceneSequence* InMovieSceneSequence, UWorld *PlaybackContext, const FActorFileHeader& ActorHeader);
	void SetComponentPossessable(UMovieSceneSequence* InMovieSceneSequence, UWorld *PlaybackContext, AActor* Actor, const FActorFileHeader& ActorHeader, const FActorProperty& ActorProperty);


private:
	TMap<FGuid, AActor*> ActorGuidToActorMap;
	bool bLoadSequenceFile;
	
};

