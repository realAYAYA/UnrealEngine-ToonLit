// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Features/IModularFeature.h"


class UMovieSceneSequence;
class UWorld;

class SERIALIZEDRECORDERINTERFACE_API  ISerializedRecorder : public IModularFeature
{
public:
	virtual ~ISerializedRecorder() {}

	static FName ModularFeatureName;
	virtual bool LoadRecordedSequencerFile(UMovieSceneSequence* InMovieSceneSequence, UWorld *PlaybackContext, const FString& InFileName, TFunction<void()> InCompletionCallback) = 0;
};

