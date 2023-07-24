// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Evaluation/MovieSceneVisibilityTemplate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneVisibilityTrack)

#define LOCTEXT_NAMESPACE "MovieSceneVisibilityTrack"


UMovieSceneVisibilityTrack::UMovieSceneVisibilityTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool UMovieSceneVisibilityTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneBoolSection::StaticClass();
}

UMovieSceneSection* UMovieSceneVisibilityTrack::CreateNewSection()
{
	UMovieSceneBoolSection* NewBoolSection = Cast<UMovieSceneBoolSection>(Super::CreateNewSection());

#if WITH_EDITORONLY_DATA
	if (NewBoolSection)
	{
		NewBoolSection->SetIsExternallyInverted(true);
	}
#endif

	return NewBoolSection;
}

FMovieSceneEvalTemplatePtr UMovieSceneVisibilityTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneVisibilitySectionTemplate(*CastChecked<const UMovieSceneBoolSection>(&InSection), *this);
}

void UMovieSceneVisibilityTrack::PostLoad()
{
#if WITH_EDITORONLY_DATA
	for (UMovieSceneSection* Section : GetAllSections())
	{
		if (Section)
		{
			CastChecked<UMovieSceneBoolSection>(Section)->SetIsExternallyInverted(true);
		}
	}
#endif

	Super::PostLoad();
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneVisibilityTrack::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Visibility");
}

#endif


#undef LOCTEXT_NAMESPACE

