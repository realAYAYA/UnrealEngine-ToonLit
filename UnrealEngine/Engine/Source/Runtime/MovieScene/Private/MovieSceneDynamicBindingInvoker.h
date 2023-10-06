// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "MovieSceneDynamicBinding.h"

class UObject;
class IMovieScenePlayer;
class UMovieSceneSequence;
struct FGuid;
struct FMovieSceneSequenceID;

/**
 * Utility class for invoking dynamic binding endpoints.
 */
struct FMovieSceneDynamicBindingInvoker
{
	/** Invoke the dynamic binding, if any, and return the result */
	static FMovieSceneDynamicBindingResolveResult ResolveDynamicBinding(IMovieScenePlayer& Player, UMovieSceneSequence* Sequence, const FMovieSceneSequenceID& SequenceID, const FGuid& InGuid, const FMovieSceneDynamicBinding& DynamicBinding);

private:
	static FMovieSceneDynamicBindingResolveResult InvokeDynamicBinding(UObject* DirectorInstance, const FMovieSceneDynamicBinding& DynamicBinding, const FMovieSceneDynamicBindingResolveParams& Params);
};

