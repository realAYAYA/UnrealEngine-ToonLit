// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneObjectPropertyTemplate.h"
#include "Tracks/MovieSceneObjectPropertyTrack.h"
#include "Sections/MovieSceneObjectPropertySection.h"
#include "UObject/StrongObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneObjectPropertyTemplate)


struct FMovieSceneObjectPropertyValue : IMovieScenePreAnimatedToken
{
	TStrongObjectPtr<UObject> Value;
	FTrackInstancePropertyBindings PropertyBindings;

	FMovieSceneObjectPropertyValue(UObject* AnimatedObject, FTrackInstancePropertyBindings* InPropertyBindings)
		: Value(InPropertyBindings->GetCurrentValue<UObject*>(*AnimatedObject))
		, PropertyBindings(*InPropertyBindings)
	{}

	virtual void RestoreState(UObject& Object, const UE::MovieScene::FRestoreStateParams& Params)
	{
		PropertyBindings.CallFunction<UObject*>(Object, Value.Get());
	}
};

struct FMovieSceneObjectPropertyValueProducer : IMovieScenePreAnimatedTokenProducer
{
	FTrackInstancePropertyBindings* PropertyBindings;
	FMovieSceneObjectPropertyValueProducer(FTrackInstancePropertyBindings* InPropertyBindings) : PropertyBindings(InPropertyBindings) {}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		return FMovieSceneObjectPropertyValue(&Object, PropertyBindings);
	}
};


struct FObjectPropertyExecToken : IMovieSceneExecutionToken
{
	FObjectPropertyExecToken(UObject* InValue)
		: NewObjectValue(MoveTemp(InValue))
	{}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		FSectionData& PropertyTrackData = PersistentData.GetSectionData<FSectionData>();
		FTrackInstancePropertyBindings* PropertyBindings = PropertyTrackData.PropertyBindings.Get();

		check(PropertyBindings);
		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			UObject* ObjectPtr = WeakObject.Get();
			if (!ObjectPtr)
			{
				continue;
			}

			FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropertyBindings->GetProperty(*ObjectPtr));
			if (!ObjectProperty || !CanAssignValue(ObjectProperty, NewObjectValue))
			{
				continue;
			}

			Player.SavePreAnimatedState(*ObjectPtr, PropertyTrackData.PropertyID, FMovieSceneObjectPropertyValueProducer(PropertyBindings));

			UObject* ExistingValue = PropertyBindings->GetCurrentValue<UObject*>(*ObjectPtr);
			if (ExistingValue != NewObjectValue)
			{
				PropertyBindings->CallFunction<UObject*>(*ObjectPtr, NewObjectValue);
			}
		}
	}

	bool CanAssignValue(FObjectPropertyBase* TargetProperty, UObject* DesiredValue) const
	{
		check(TargetProperty);
		if (!TargetProperty->PropertyClass)
		{
			return false;
		}
		else if (!DesiredValue)
		{
			return !TargetProperty->HasAnyPropertyFlags(CPF_NoClear);
		}
		else if (DesiredValue->GetClass() != nullptr)
		{
			return DesiredValue->GetClass()->IsChildOf(TargetProperty->PropertyClass);
		}
		return false;
	}

	UObject* NewObjectValue;
};


FMovieSceneObjectPropertyTemplate::FMovieSceneObjectPropertyTemplate(const UMovieSceneObjectPropertySection& Section, const UMovieSceneObjectPropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath().ToString())
	, ObjectChannel(Section.ObjectChannel)
{}

void FMovieSceneObjectPropertyTemplate::SetupOverrides()
{
	// We need FMovieScenePropertySectionTemplate::Setup to be called for initialization of the track instance bindings
	EnableOverrides(RequiresSetupFlag);
}

void FMovieSceneObjectPropertyTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	UObject* Object = nullptr;
	if (ObjectChannel.Evaluate(Context.GetTime(), Object))
	{
		ExecutionTokens.Add(FObjectPropertyExecToken(Object));
	}
}

