// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameTime.h"
#include "MovieSceneTimeController.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "IMovieSceneCustomClockSource.generated.h"

class UObject;
struct FFrame;
struct FQualifiedFrameTime;

UINTERFACE()
class MOVIESCENE_API UMovieSceneCustomClockSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class IMovieSceneCustomClockSource
{
public:

	GENERATED_BODY()

	UFUNCTION()
	virtual void OnTick(float DeltaSeconds, float InPlayRate) {}

	UFUNCTION()
	virtual void OnStartPlaying(const FQualifiedFrameTime& InStartTime) {}

	UFUNCTION()
	virtual void OnStopPlaying(const FQualifiedFrameTime& InStopTime) {}

	UFUNCTION()
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) { return 0; }
};

struct MOVIESCENE_API FMovieSceneTimeController_Custom : FMovieSceneTimeController
{
	explicit FMovieSceneTimeController_Custom(const FSoftObjectPath& InObjectPath, TWeakObjectPtr<> PlaybackContext);

private:

	virtual void OnTick(float DeltaSeconds, float InPlayRate) override final;
	virtual void OnStartPlaying(const FQualifiedFrameTime& InStartTime) override final;
	virtual void OnStopPlaying(const FQualifiedFrameTime& InStopTime) override final;
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) override final;

private:

	void ResolveInterfacePtr();

	TWeakObjectPtr<> WeakPlaybackContext;

	TWeakObjectPtr<> WeakObject;

	IMovieSceneCustomClockSource* InterfacePtr;

	FSoftObjectPath ObjectPath;
};