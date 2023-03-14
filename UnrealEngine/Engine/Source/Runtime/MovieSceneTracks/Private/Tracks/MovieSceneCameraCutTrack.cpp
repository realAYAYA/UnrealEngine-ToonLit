// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneTimeHelpers.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "IMovieSceneTracksModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneCameraCutTrack)

#define LOCTEXT_NAMESPACE "MovieSceneCameraCutTrack"

/* UMovieSceneCameraCutTrack interface
 *****************************************************************************/
UMovieSceneCameraCutTrack::UMovieSceneCameraCutTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
	, bCanBlend(false)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(120, 120, 120, 65);
#endif

	// By default, don't evaluate camera cuts in pre and postroll
	EvalOptions.bEvaluateInPreroll = EvalOptions.bEvaluateInPostroll = false;

	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);
}

UMovieSceneCameraCutSection* UMovieSceneCameraCutTrack::AddNewCameraCut(const FMovieSceneObjectBindingID& CameraBindingID, FFrameNumber StartTime)
{
	Modify();

	FFrameNumber NewSectionEndTime = FindEndTimeForCameraCut(StartTime);

	// If there's an existing section, just swap the camera guid
	UMovieSceneCameraCutSection* ExistingSection = nullptr;
	for (auto Section : Sections)
	{
		if (Section->HasStartFrame() && Section->HasEndFrame() && Section->GetInclusiveStartFrame() == StartTime && Section->GetExclusiveEndFrame() == NewSectionEndTime)
		{
			ExistingSection = Cast<UMovieSceneCameraCutSection>(Section);
			break;
		}
	}

	UMovieSceneCameraCutSection* NewSection = ExistingSection;
	if (ExistingSection != nullptr)
	{
		ExistingSection->SetCameraBindingID(CameraBindingID);
	}
	else
	{
		NewSection = NewObject<UMovieSceneCameraCutSection>(this, NAME_None, RF_Transactional);
		NewSection->SetRange(TRange<FFrameNumber>(StartTime, NewSectionEndTime));
		NewSection->SetCameraBindingID(CameraBindingID);

		AddSection(*NewSection);
	}

	// When a new CameraCut is added, sort all CameraCuts to ensure they are in the correct order
	MovieSceneHelpers::SortConsecutiveSections(Sections);

	// Once CameraCuts are sorted fixup the surrounding CameraCuts to fix any gaps
	if (bCanBlend)
	{
		MovieSceneHelpers::FixupConsecutiveBlendingSections(Sections, *NewSection, false);
	}
	else
	{
		MovieSceneHelpers::FixupConsecutiveSections(Sections, *NewSection, false);
	}

	return NewSection;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneCameraCutTrack::AddSection(UMovieSceneSection& Section)
{
	if (UMovieSceneCameraCutSection* CutSection = Cast<UMovieSceneCameraCutSection>(&Section))
	{
		Sections.Add(CutSection);
	}
}

bool UMovieSceneCameraCutTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneCameraCutSection::StaticClass();
}

UMovieSceneSection* UMovieSceneCameraCutTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCameraCutSection>(this, NAME_None, RF_Transactional);
}

bool UMovieSceneCameraCutTrack::SupportsMultipleRows() const
{
	return false;
}

EMovieSceneTrackEasingSupportFlags UMovieSceneCameraCutTrack::SupportsEasing(FMovieSceneSupportsEasingParams& Params) const
{
	if (!bCanBlend)
	{
		return EMovieSceneTrackEasingSupportFlags::None;
	}
	if (Params.ForSection != nullptr)
	{
		const int32 NumSections = Sections.Num();
		if (NumSections == 1)
		{
			return EMovieSceneTrackEasingSupportFlags::AutomaticEasing | EMovieSceneTrackEasingSupportFlags::ManualEasing;
		}
		else if (NumSections > 1)
		{
			// Find the section with the earliest start time, and the section with the latest end time.
			int32 EdgeSections[2] = { 0, NumSections - 1 };
			TRange<FFrameNumber> EdgeRanges[2] = { Sections[0]->GetTrueRange(), Sections[NumSections - 1]->GetTrueRange() };
			const TRange<FFrameNumber> ForSectionRange = Params.ForSection->GetTrueRange();
			for (int32 Index = 0; Index < NumSections; ++Index)
			{
				const UMovieSceneSection* Section(Sections[Index]);
				const TRange<FFrameNumber> SectionRange(Section->GetTrueRange());
				if (Section != Params.ForSection && SectionRange.Contains(ForSectionRange))
				{
					return EMovieSceneTrackEasingSupportFlags::None;
				}

				if (EdgeRanges[0].HasLowerBound())
				{
					if (!SectionRange.HasLowerBound() || SectionRange.GetLowerBoundValue() < EdgeRanges[0].GetLowerBoundValue())
					{
						EdgeSections[0] = Index;
						EdgeRanges[0] = SectionRange;
					}
				}
				if (EdgeRanges[1].HasUpperBound())
				{
					if (!SectionRange.HasUpperBound() || SectionRange.GetUpperBoundValue() > EdgeRanges[1].GetUpperBoundValue())
					{
						EdgeSections[1] = Index;
						EdgeRanges[1] = SectionRange;
					}
				}
			}
			
			// Allow easing for these sections.
			EMovieSceneTrackEasingSupportFlags Flags = EMovieSceneTrackEasingSupportFlags::AutomaticEasing;
			if (Params.ForSection == Sections[EdgeSections[0]])
			{
				Flags |= EMovieSceneTrackEasingSupportFlags::ManualEaseIn;
			}
			if (Params.ForSection == Sections[EdgeSections[1]])
			{
				Flags |= EMovieSceneTrackEasingSupportFlags::ManualEaseOut;
			}
			return Flags;
		}
	}
	return EMovieSceneTrackEasingSupportFlags::AutomaticEasing;
}

const TArray<UMovieSceneSection*>& UMovieSceneCameraCutTrack::GetAllSections() const
{
	return Sections;
}

bool UMovieSceneCameraCutTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


bool UMovieSceneCameraCutTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

void UMovieSceneCameraCutTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);

	if (bCanBlend)
	{
		MovieSceneHelpers::FixupConsecutiveBlendingSections(Sections, Section, true);
	}
	else
	{
		MovieSceneHelpers::FixupConsecutiveSections(Sections, Section, true);
	}

	// @todo Sequencer: The movie scene owned by the section is now abandoned.  Should we offer to delete it?  
}

void UMovieSceneCameraCutTrack::RemoveSectionAt(int32 SectionIndex)
{
	UMovieSceneSection* SectionToDelete = Sections[SectionIndex];
	if (bCanBlend)
	{
		MovieSceneHelpers::FixupConsecutiveBlendingSections(Sections, *SectionToDelete, true);
	}
	else
	{
		MovieSceneHelpers::FixupConsecutiveSections(Sections, *SectionToDelete, true);
	}

	Sections.RemoveAt(SectionIndex);
	MovieSceneHelpers::SortConsecutiveSections(Sections);
}

void UMovieSceneCameraCutTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneCameraCutTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Camera Cuts");
}
#endif


#if WITH_EDITOR
EMovieSceneSectionMovedResult UMovieSceneCameraCutTrack::OnSectionMoved(UMovieSceneSection& Section, const FMovieSceneSectionMovedParams& Params)
{
	const bool bCleanUp = (Params.MoveType == EPropertyChangeType::ValueSet);
	bool bCleanUpDone = false;
	if (bCanBlend)
	{
		bCleanUpDone = MovieSceneHelpers::FixupConsecutiveBlendingSections(Sections, Section, false, bCleanUp);
	}
	else
	{
		bCleanUpDone = MovieSceneHelpers::FixupConsecutiveSections(Sections, Section, false, bCleanUp);
	}

	return bCleanUpDone ? EMovieSceneSectionMovedResult::SectionsChanged : EMovieSceneSectionMovedResult::None;
}
#endif

FFrameNumber UMovieSceneCameraCutTrack::FindEndTimeForCameraCut( FFrameNumber StartTime )
{
	UMovieScene* OwnerScene = GetTypedOuter<UMovieScene>();

	// End time should default to end where the movie scene ends. Ensure it is at least the same as start time (this should only happen when the movie scene has an initial time range smaller than the start time)
	FFrameNumber ExclusivePlayEnd = UE::MovieScene::DiscreteExclusiveUpper(OwnerScene->GetPlaybackRange());
	FFrameNumber ExclusiveEndTime = FMath::Max( ExclusivePlayEnd, StartTime );

	for( UMovieSceneSection* Section : Sections )
	{
		if( Section->HasStartFrame() && Section->GetInclusiveStartFrame() > StartTime )
		{
			ExclusiveEndTime = Section->GetInclusiveStartFrame();
			break;
		}
	}

	if( StartTime == ExclusiveEndTime )
	{
		// Give the CameraCut a reasonable length of time to start out with.  A 0 time CameraCut is not usable
		ExclusiveEndTime = (StartTime + .5f * OwnerScene->GetTickResolution()).FrameNumber;
	}

	return ExclusiveEndTime;
}

void UMovieSceneCameraCutTrack::PreCompileImpl(FMovieSceneTrackPreCompileResult& OutPreCompileResult)
{
	for (UMovieSceneSection* Section : Sections)
	{
		if (UMovieSceneCameraCutSection* CameraCutSection = CastChecked<UMovieSceneCameraCutSection>(Section))
		{
			CameraCutSection->ComputeInitialCameraCutTransform();
		}
	}
}

#undef LOCTEXT_NAMESPACE

