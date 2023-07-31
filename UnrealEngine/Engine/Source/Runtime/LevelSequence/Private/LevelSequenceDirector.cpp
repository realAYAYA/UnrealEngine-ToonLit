// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceDirector.h"
#include "Engine/World.h"
#include "UObject/Stack.h"
#include "MovieSceneObjectBindingID.h"
#include "LevelSequencePlayer.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneSequence.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "Evaluation/MovieScenePlayback.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelSequenceDirector)

UWorld* ULevelSequenceDirector::GetWorld() const
{
	if (ULevel* OuterLevel = GetTypedOuter<ULevel>())
	{
		return OuterLevel->OwningWorld;
	}
	return GetTypedOuter<UWorld>();
}

FQualifiedFrameTime ULevelSequenceDirector::GetMasterSequenceTime() const
{
	using namespace UE::MovieScene;

	if (IMovieScenePlayer* PlayerInterface = IMovieScenePlayer::Get(static_cast<uint16>(MovieScenePlayerIndex)))
	{
		FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = PlayerInterface->GetEvaluationTemplate();
		UMovieSceneSequence* RootSequence = EvaluationTemplate.GetRootSequence();
		const FSequenceInstance* RootSequenceInstance = EvaluationTemplate.FindInstance(MovieSceneSequenceID::Root);
		if (RootSequenceInstance && RootSequence)
		{
			// Put the qualified frame time into 'display' rate
			FFrameRate DisplayRate = RootSequence->GetMovieScene()->GetDisplayRate();
			FMovieSceneContext Context = RootSequenceInstance->GetContext();

			FFrameTime DisplayRateTime = ConvertFrameTime(Context.GetTime(), Context.GetFrameRate(), DisplayRate);
			return FQualifiedFrameTime(DisplayRateTime, DisplayRate);
		}
	}
	return FQualifiedFrameTime(0, FFrameRate());
}

FQualifiedFrameTime ULevelSequenceDirector::GetCurrentTime() const
{
	using namespace UE::MovieScene;

	if (IMovieScenePlayer* PlayerInterface = IMovieScenePlayer::Get(static_cast<uint16>(MovieScenePlayerIndex)))
	{
		FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = PlayerInterface->GetEvaluationTemplate();

		UMovieSceneSequence* SubSequence = EvaluationTemplate.GetSequence(FMovieSceneSequenceID(SubSequenceID));
		const FSequenceInstance* SequenceInstance = EvaluationTemplate.FindInstance(FMovieSceneSequenceID(SubSequenceID));
		if (SequenceInstance && SubSequence)
		{
			// Put the qualified frame time into 'display' rate
			FFrameRate DisplayRate = SubSequence->GetMovieScene()->GetDisplayRate();
			FMovieSceneContext Context = SequenceInstance->GetContext();

			FFrameTime DisplayRateTime = ConvertFrameTime(Context.GetTime(), Context.GetFrameRate(), DisplayRate);
			return FQualifiedFrameTime(DisplayRateTime, DisplayRate);
		}
	}
	return FQualifiedFrameTime(0, FFrameRate());
}


TArray<UObject*> ULevelSequenceDirector::GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding)
{
	TArray<UObject*> Objects;

	if (IMovieScenePlayer* PlayerInterface = IMovieScenePlayer::Get(static_cast<uint16>(MovieScenePlayerIndex)))
	{
		for (TWeakObjectPtr<> WeakObject : ObjectBinding.ResolveBoundObjects(FMovieSceneSequenceID(SubSequenceID), *PlayerInterface))
		{
			if (UObject* Object = WeakObject.Get())
			{
				Objects.Add(Object);
			}
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("No player interface available or assigned."), ELogVerbosity::Error);
	}

	return Objects;
}

UObject* ULevelSequenceDirector::GetBoundObject(FMovieSceneObjectBindingID ObjectBinding)
{
	if (IMovieScenePlayer* PlayerInterface = IMovieScenePlayer::Get(static_cast<uint16>(MovieScenePlayerIndex)))
	{
		for (TWeakObjectPtr<> WeakObject : ObjectBinding.ResolveBoundObjects(FMovieSceneSequenceID(SubSequenceID), *PlayerInterface))
		{
			if (UObject* Object = WeakObject.Get())
			{
				return Object;
			}
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("No player interface available or assigned."), ELogVerbosity::Error);
	}

	return nullptr;
}

TArray<AActor*> ULevelSequenceDirector::GetBoundActors(FMovieSceneObjectBindingID ObjectBinding)
{
	TArray<AActor*> Actors;

	if (IMovieScenePlayer* PlayerInterface = IMovieScenePlayer::Get(static_cast<uint16>(MovieScenePlayerIndex)))
	{
		for (TWeakObjectPtr<> WeakObject : ObjectBinding.ResolveBoundObjects(FMovieSceneSequenceID(SubSequenceID), *PlayerInterface))
		{
			if (AActor* Actor = Cast<AActor>(WeakObject.Get()))
			{
				Actors.Add(Actor);
			}
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("No player interface available or assigned."), ELogVerbosity::Error);
	}

	return Actors;
}

AActor* ULevelSequenceDirector::GetBoundActor(FMovieSceneObjectBindingID ObjectBinding)
{
	if (IMovieScenePlayer* PlayerInterface = IMovieScenePlayer::Get(static_cast<uint16>(MovieScenePlayerIndex)))
	{
		for (TWeakObjectPtr<> WeakObject : ObjectBinding.ResolveBoundObjects(FMovieSceneSequenceID(SubSequenceID), *PlayerInterface))
		{
			if (AActor* Actor = Cast<AActor>(WeakObject.Get()))
			{
				return Actor;
			}
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("No player interface available or assigned."), ELogVerbosity::Error);
	}

	return nullptr;
}

UMovieSceneSequence* ULevelSequenceDirector::GetSequence()
{
	if (IMovieScenePlayer* PlayerInterface = IMovieScenePlayer::Get(static_cast<uint16>(MovieScenePlayerIndex)))
	{
		return PlayerInterface->GetEvaluationTemplate().GetSequence(FMovieSceneSequenceID(SubSequenceID));
	}
	else
	{		
		FFrame::KismetExecutionMessage(TEXT("No sequence player."), ELogVerbosity::Error);

		return nullptr;
	}
}


