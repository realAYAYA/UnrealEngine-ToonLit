// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "MovieSceneSequenceTickManagerClient.generated.h"


class FMovieSceneEntitySystemRunner;

/**
 * Interface for objects that are to be ticked by the tick manager.
 */
UINTERFACE(MinimalAPI)
class UMovieSceneSequenceTickManagerClient
	: public UInterface
{
public:
	GENERATED_BODY()
};

class IMovieSceneSequenceTickManagerClient
{
public:
	GENERATED_BODY()

	virtual void TickFromSequenceTickManager(float DeltaSeconds, FMovieSceneEntitySystemRunner* Runner) = 0;
};

/** Deprecated IMovieSceneSequenceActor interface that has been replaced with IMovieSceneSequenceTickManagerClient */
class UE_DEPRECATED(5.1, "Please use IMovieSceneSequenceTickManagerClient directly") IMovieSceneSequenceActor
	: public IMovieSceneSequenceTickManagerClient
{
	virtual void TickFromSequenceTickManager(float DeltaSeconds) = 0;
	virtual void TickFromSequenceTickManager(float DeltaSeconds, FMovieSceneEntitySystemRunner* Runner) override
	{
		TickFromSequenceTickManager(DeltaSeconds);
	}
};
