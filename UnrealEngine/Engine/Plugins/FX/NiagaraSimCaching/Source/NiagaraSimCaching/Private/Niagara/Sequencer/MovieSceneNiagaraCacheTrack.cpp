// Copyright Epic Games, Inc. All Rights Reserved.

#include "Niagara/Sequencer/MovieSceneNiagaraCacheTrack.h"
#include "MovieScene.h"
#include "Niagara/Sequencer/MovieSceneNiagaraCacheSection.h"
#include "MovieSceneNiagaraCacheTemplate.h"
#include "NiagaraSimCache.h"

#define LOCTEXT_NAMESPACE "MovieSceneNiagaraCacheTrack"

/* UMovieSceneNiagaraCacheTrack constructor
 *****************************************************************************/

UMovieSceneNiagaraCacheTrack::UMovieSceneNiagaraCacheTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(19, 58, 171, 80);
#endif

	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);

	EvalOptions.bCanEvaluateNearestSection = true;
	EvalOptions.bEvaluateInPreroll = true;
}

/* UMovieSceneNiagaraCacheTrack interface
 *****************************************************************************/

UMovieSceneSection* UMovieSceneNiagaraCacheTrack::AddNewAnimation(FFrameNumber KeyTime, UNiagaraComponent* NiagaraComponent)
{
	UMovieSceneNiagaraCacheSection* NewSection = Cast<UMovieSceneNiagaraCacheSection>(CreateNewSection());
	{
		//const FFrameTime AnimationLength = NiagaraCache->CacheCollection->GetMaxDuration() * GetTypedOuter<UMovieScene>()->GetTickResolution();
		//const int32 IFrameNumber = AnimationLength.FrameNumber.Value + (int)(AnimationLength.GetSubFrame() + 0.5f) + 1;
		//NewSection->InitialPlacementOnRow(AnimationSections, KeyTime, IFrameNumber, INDEX_NONE);
	}

	AddSection(*NewSection);

	return NewSection;
}

TArray<UMovieSceneSection*> UMovieSceneNiagaraCacheTrack::GetAnimSectionsAtTime(FFrameNumber Time)
{
	TArray<UMovieSceneSection*> Sections;
	for (auto Section : AnimationSections)
	{
		if (Section->IsTimeWithinSection(Time))
		{
			Sections.Add(Section);
		}
	}

	return Sections;
}

FMovieSceneEvalTemplatePtr UMovieSceneNiagaraCacheTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneNiagaraCacheSectionTemplate(*CastChecked<UMovieSceneNiagaraCacheSection>(&InSection));
}

/* UMovieSceneTrack interface
 *****************************************************************************/

const TArray<UMovieSceneSection*>& UMovieSceneNiagaraCacheTrack::GetAllSections() const
{
	return AnimationSections;
}

bool UMovieSceneNiagaraCacheTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneNiagaraCacheSection::StaticClass();
}

UMovieSceneSection* UMovieSceneNiagaraCacheTrack::CreateNewSection()
{
	UMovieSceneNiagaraCacheSection* MovieSceneNiagaraCacheSection = NewObject<UMovieSceneNiagaraCacheSection>(this, NAME_None, RF_Transactional);
	MovieSceneNiagaraCacheSection->Params.SimCache = NewObject<UNiagaraSimCache>(MovieSceneNiagaraCacheSection, NAME_None, RF_Transactional);
	MovieSceneNiagaraCacheSection->Params.CacheParameters.AttributeCaptureMode = ENiagaraSimCacheAttributeCaptureMode::RenderingOnly;
	return MovieSceneNiagaraCacheSection;
}

void UMovieSceneNiagaraCacheTrack::RemoveAllAnimationData()
{
	AnimationSections.Empty();
}

bool UMovieSceneNiagaraCacheTrack::HasSection(const UMovieSceneSection& Section) const
{
	return AnimationSections.Contains(&Section);
}

void UMovieSceneNiagaraCacheTrack::AddSection(UMovieSceneSection& Section)
{
	AnimationSections.Add(&Section);
}

void UMovieSceneNiagaraCacheTrack::RemoveSection(UMovieSceneSection& Section)
{
	AnimationSections.Remove(&Section);
}

void UMovieSceneNiagaraCacheTrack::RemoveSectionAt(int32 SectionIndex)
{
	AnimationSections.RemoveAt(SectionIndex);
}

bool UMovieSceneNiagaraCacheTrack::IsEmpty() const
{
	return AnimationSections.Num() == 0;
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneNiagaraCacheTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Niagara Sim Cache");
}

#endif

#undef LOCTEXT_NAMESPACE
