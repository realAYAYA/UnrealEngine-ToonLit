// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneBindingLifetimeSection.h"
#include "Tracks/MovieSceneBindingLifetimeTrack.h"

#include "EntitySystem/MovieSceneEntityBuilder.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "Evaluation/MovieSceneEvaluationField.h"
#include "MovieScene.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBindingLifetimeSection)

UMovieSceneBindingLifetimeSection::UMovieSceneBindingLifetimeSection(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	bSupportsInfiniteRange = true;
}

void UMovieSceneBindingLifetimeSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponentTypes = FBuiltInComponentTypes::Get();

	OutImportedEntity->AddBuilder(
		FEntityBuilder()
		.AddConditional(BuiltInComponentTypes->BindingLifetime, FMovieSceneBindingLifetimeComponentData{ Params.GetObjectBindingID(), EMovieSceneBindingLifetimeState::Active }, Params.GetObjectBindingID().IsValid())
	);
}

bool UMovieSceneBindingLifetimeSection::PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	// By default, Binding Lifetime sections do not populate any evaluation field entries
	// that is the job of its outer UMovieSceneTrack through a call to ExternalPopulateEvaluationField
	return true;
}

void UMovieSceneBindingLifetimeSection::ExternalPopulateEvaluationField(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	using namespace UE::MovieScene;

	const int32 MetaDataIndex = OutFieldBuilder->AddMetaData(InMetaData);

	const int32 EntityIndex = OutFieldBuilder->FindOrAddEntity(this, 1);
	OutFieldBuilder->AddPersistentEntity(EffectiveRange, EntityIndex, MetaDataIndex);
}

void UMovieSceneBindingLifetimeSection::InitialPlacement(const TArray<UMovieSceneSection*>& Sections, FFrameNumber InStartTime, int32 Duration, bool bAllowMultipleRows)
{
	// We don't allow overlap or multi row here, so need a few extra rules for initial placement

	check(Duration >= 0);

	// Inclusive lower, exclusive upper bounds
	SectionRange = TRange<FFrameNumber>(InStartTime, InStartTime + Duration);

	for (;;)
	{
		UMovieSceneSection* OverlappedSection = const_cast<UMovieSceneSection*>(OverlapsWithSections(Sections));
		if (OverlappedSection == nullptr)
		{
			break;
		}

		TRange<FFrameNumber> OtherRange = OverlappedSection->GetRange();

		if (OtherRange.GetUpperBound().IsClosed())
		{
			MoveSection(OtherRange.GetUpperBoundValue() - InStartTime);
		}
		else
		{
			// Edge case- the other range has an infinite end, but can't be split because the point it would be split at is its start.
			if (OtherRange.GetLowerBound().IsClosed() && OtherRange.GetLowerBoundValue() == SectionRange.GetLowerBound().GetValue())
			{
				// Adjust the lowerbound of OtherRange
				OtherRange.SetLowerBoundValue(SectionRange.GetUpperBound().GetValue());
				OverlappedSection->SetRange(OtherRange);
			}
			else
			{
				// We're overlapping an at-least partially infinite section here. Split it.
				UMovieSceneSection* NewSection = OverlappedSection->SplitSection(FQualifiedFrameTime(SectionRange.GetLowerBound().GetValue(), GetTypedOuter<UMovieScene>()->GetTickResolution()), false);
				// The new section will be the one on the right and will still have an open upper bound. Resize it to fit the new section.
				TRange<FFrameNumber> NewSectionRange = NewSection->GetRange();
				NewSectionRange.SetLowerBoundValue(SectionRange.GetUpperBound().GetValue());
				NewSection->SetRange(NewSectionRange);
			}
		}
	}
}

