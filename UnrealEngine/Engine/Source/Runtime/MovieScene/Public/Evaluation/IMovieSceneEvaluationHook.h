// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "MovieSceneSequenceID.h"
#include "Misc/Guid.h"
#include "Evaluation/MovieScenePlayback.h"
#include "IMovieSceneEvaluationHook.generated.h"

class IMovieScenePlayer;
class UMovieSceneEntitySystemLinker;

namespace UE
{
namespace MovieScene
{


enum class EEvaluationHookEvent
{
	Begin,
	Update,
	End,

	Trigger,
};

struct FEvaluationHookParams
{
	/** The object binding ID for the hook */
	FGuid ObjectBindingID;

	/** Evaluation context */
	FMovieSceneContext Context;

	/** The sequence ID for the hook */
	FMovieSceneSequenceID SequenceID = MovieSceneSequenceID::Root;

	int32 TriggerIndex = INDEX_NONE;
};


} // namespace MovieScene
} // namespace UE


UINTERFACE()
class MOVIESCENE_API UMovieSceneEvaluationHook : public UInterface
{
public:
	GENERATED_BODY()
};


/**
 * All evaluation hooks are executed at the end of the frame (at a time when re-entrancy is permitted), and cannot have any component dependencies
 */
class MOVIESCENE_API IMovieSceneEvaluationHook
{
public:

	GENERATED_BODY()

	virtual void Begin(IMovieScenePlayer* Player, const UE::MovieScene::FEvaluationHookParams& Params) const {}
	virtual void Update(IMovieScenePlayer* Player, const UE::MovieScene::FEvaluationHookParams& Params) const {}
	virtual void End(IMovieScenePlayer* Player, const UE::MovieScene::FEvaluationHookParams& Params) const {}

	virtual void Trigger(IMovieScenePlayer* Player, const UE::MovieScene::FEvaluationHookParams& Params) const {}
};
