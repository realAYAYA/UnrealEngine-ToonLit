// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneActorReferenceTemplate.h"

#include "Sections/MovieSceneActorReferenceSection.h"
#include "Tracks/MovieSceneActorReferenceTrack.h"
#include "GameFramework/Actor.h"
#include "Evaluation/MovieSceneEvaluation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneActorReferenceTemplate)


namespace PropertyTemplate
{
	template<>
	UObject* ConvertFromIntermediateType<UObject*, FMovieSceneObjectBindingID>(const FMovieSceneObjectBindingID& InObjectBinding, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		for (TWeakObjectPtr<> WeakObject : InObjectBinding.ResolveBoundObjects(Operand.SequenceID, Player))
		{
			if (AActor* Actor = Cast<AActor>(WeakObject.Get()))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	template<>
	UObject* ConvertFromIntermediateType<UObject*, TWeakObjectPtr<>>(const TWeakObjectPtr<>& InWeakPtr, IMovieScenePlayer& Player)
	{
		return InWeakPtr.Get();
	}

	template<>
	UObject* ConvertFromIntermediateType<UObject*, TWeakObjectPtr<>>(const TWeakObjectPtr<>& InWeakPtr, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		return InWeakPtr.Get();
	}

	template<> IMovieScenePreAnimatedTokenPtr CacheExistingState<UObject*, FMovieSceneObjectBindingID>(UObject& Object, FTrackInstancePropertyBindings& PropertyBindings)
	{
		return TCachedState<UObject*, TWeakObjectPtr<>>(PropertyBindings.GetCurrentValue<UObject*>(Object), PropertyBindings);
	}
}

FMovieSceneActorReferenceSectionTemplate::FMovieSceneActorReferenceSectionTemplate(const UMovieSceneActorReferenceSection& Section, const UMovieScenePropertyTrack& Track)
	: PropertyData(Track.GetPropertyName(), Track.GetPropertyPath().ToString())
	, ActorReferenceData(Section.GetActorReferenceData())
{
}

void FMovieSceneActorReferenceSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	using namespace PropertyTemplate;

	FMovieSceneActorReferenceKey ObjectBinding;
	ActorReferenceData.Evaluate(Context.GetTime(), ObjectBinding);
	ExecutionTokens.Add(TPropertyTrackExecutionToken<UObject*, FMovieSceneObjectBindingID>(ObjectBinding.Object));
}

