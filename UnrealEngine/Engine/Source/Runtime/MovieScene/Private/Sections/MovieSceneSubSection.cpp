// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSubSection.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTimeHelpers.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Misc/FrameRate.h"
#include "Logging/MessageLog.h"
#include "Evaluation/MovieSceneRootOverridePath.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSubSection)

float DeprecatedMagicNumber = TNumericLimits<float>::Lowest();

/* UMovieSceneSubSection structors
 *****************************************************************************/

UMovieSceneSubSection::UMovieSceneSubSection(const FObjectInitializer& ObjInitializer)
	: Super(ObjInitializer)
	, StartOffset_DEPRECATED(DeprecatedMagicNumber)
	, TimeScale_DEPRECATED(DeprecatedMagicNumber)
	, PrerollTime_DEPRECATED(DeprecatedMagicNumber)
{
	NetworkMask = (uint8)(EMovieSceneServerClientMask::Server | EMovieSceneServerClientMask::Client);
}

FMovieSceneSequenceTransform UMovieSceneSubSection::OuterToInnerTransform() const
{
	UMovieSceneSequence* SequencePtr   = GetSequence();
	if (!SequencePtr)
	{
		return FMovieSceneSequenceTransform();
	}

	UMovieScene* MovieScenePtr = SequencePtr->GetMovieScene();

	TRange<FFrameNumber> SubRange = GetRange();
	if (!MovieScenePtr || SubRange.GetLowerBound().IsOpen())
	{
		return FMovieSceneSequenceTransform();
	}

	const FFrameRate   InnerFrameRate = MovieScenePtr->GetTickResolution();
	const FFrameRate   OuterFrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();
	const float        FrameRateScale = (OuterFrameRate == InnerFrameRate) ? 1.f : (InnerFrameRate / OuterFrameRate).AsDecimal();

	const TRange<FFrameNumber> MovieScenePlaybackRange = GetValidatedInnerPlaybackRange(Parameters, *MovieScenePtr);
	const FFrameNumber InnerStartTime = UE::MovieScene::DiscreteInclusiveLower(MovieScenePlaybackRange);
	const FFrameNumber OuterStartTime = UE::MovieScene::DiscreteInclusiveLower(SubRange);

	// This is the transform for the "placement" (position and scaling) of the sub-sequence.
	FMovieSceneTimeTransform LinearTransform =
		// Inner play offset
		FMovieSceneTimeTransform(InnerStartTime)
		// Inner play rate
		* FMovieSceneTimeTransform(0, Parameters.TimeScale * FrameRateScale)
		// Outer section start time
		* FMovieSceneTimeTransform(-OuterStartTime);
	
	if (!Parameters.bCanLoop)
	{
		return FMovieSceneSequenceTransform(LinearTransform);
	}
	else
	{
		const FFrameNumber InnerEndTime = UE::MovieScene::DiscreteExclusiveUpper(MovieScenePlaybackRange);
		const FMovieSceneTimeWarping LoopingTransform(InnerStartTime, InnerEndTime);
		LinearTransform = FMovieSceneTimeTransform(Parameters.FirstLoopStartFrameOffset) * LinearTransform;

		FMovieSceneSequenceTransform Result;
		Result.NestedTransforms.Add(FMovieSceneNestedSequenceTransform(LinearTransform, LoopingTransform));
		return Result;
	}
}

bool UMovieSceneSubSection::GetValidatedInnerPlaybackRange(TRange<FFrameNumber>& OutInnerPlaybackRange) const
{
	UMovieSceneSequence* SequencePtr = GetSequence();
	if (SequencePtr != nullptr)
	{
		UMovieScene* MovieScenePtr = SequencePtr->GetMovieScene();
		if (MovieScenePtr != nullptr)
		{
			OutInnerPlaybackRange = GetValidatedInnerPlaybackRange(Parameters, *MovieScenePtr);
			return true;
		}
	}
	return false;
}

TRange<FFrameNumber> UMovieSceneSubSection::GetValidatedInnerPlaybackRange(const FMovieSceneSectionParameters& SubSectionParameters, const UMovieScene& InnerMovieScene)
{
	const TRange<FFrameNumber> InnerPlaybackRange = InnerMovieScene.GetPlaybackRange();
	TRangeBound<FFrameNumber> ValidatedLowerBound = InnerPlaybackRange.GetLowerBound();
	TRangeBound<FFrameNumber> ValidatedUpperBound = InnerPlaybackRange.GetUpperBound();
	if (ValidatedLowerBound.IsClosed() && ValidatedUpperBound.IsClosed())
	{
		const FFrameRate TickResolution = InnerMovieScene.GetTickResolution();
		const FFrameRate DisplayRate = InnerMovieScene.GetDisplayRate();
		const FFrameNumber OneFrameInTicks = FFrameRate::TransformTime(FFrameTime(1), DisplayRate, TickResolution).FloorToFrame();

		ValidatedLowerBound.SetValue(ValidatedLowerBound.GetValue() + SubSectionParameters.StartFrameOffset);
		ValidatedUpperBound.SetValue(FMath::Max(ValidatedUpperBound.GetValue() - SubSectionParameters.EndFrameOffset, ValidatedLowerBound.GetValue() + OneFrameInTicks));
		return TRange<FFrameNumber>(ValidatedLowerBound, ValidatedUpperBound);
	}
	return InnerPlaybackRange;
}

FString UMovieSceneSubSection::GetPathNameInMovieScene() const
{
	UMovieScene* OuterMovieScene = GetTypedOuter<UMovieScene>();
	check(OuterMovieScene);
	return GetPathName(OuterMovieScene);
}

FMovieSceneSequenceID UMovieSceneSubSection::GetSequenceID() const
{
	FString FullPath = GetPathNameInMovieScene();
	if (SubSequence)
	{
		FullPath += TEXT(" / ");
		FullPath += SubSequence->GetPathName();
	}

	return FMovieSceneSequenceID(FCrc::Strihash_DEPRECATED(*FullPath));
}

void UMovieSceneSubSection::PostLoad()
{
	FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

	TOptional<double> StartOffsetToUpgrade;
	if (StartOffset_DEPRECATED != DeprecatedMagicNumber)
	{
		StartOffsetToUpgrade = StartOffset_DEPRECATED;

		StartOffset_DEPRECATED = DeprecatedMagicNumber;
	}
	else if (Parameters.StartOffset_DEPRECATED != 0.f)
	{
		StartOffsetToUpgrade = Parameters.StartOffset_DEPRECATED;
	}

	if (StartOffsetToUpgrade.IsSet())
	{
		FFrameNumber StartFrame = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, StartOffsetToUpgrade.GetValue());
		Parameters.StartFrameOffset = StartFrame;
	}

	if (TimeScale_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.TimeScale = TimeScale_DEPRECATED;

		TimeScale_DEPRECATED = DeprecatedMagicNumber;
	}

	if (PrerollTime_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.PrerollTime_DEPRECATED = PrerollTime_DEPRECATED;

		PrerollTime_DEPRECATED = DeprecatedMagicNumber;
	}

	// Pre and post roll is now supported generically
	if (Parameters.PrerollTime_DEPRECATED > 0.f)
	{
		FFrameNumber ClampedPreRollFrames = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Parameters.PrerollTime_DEPRECATED);
		SetPreRollFrames(ClampedPreRollFrames.Value);
	}

	if (Parameters.PostrollTime_DEPRECATED > 0.f)
	{
		FFrameNumber ClampedPostRollFrames = UpgradeLegacyMovieSceneTime(this, LegacyFrameRate, Parameters.PostrollTime_DEPRECATED);
		SetPreRollFrames(ClampedPostRollFrames.Value);
	}

	Super::PostLoad();
}

void UMovieSceneSubSection::SetSequence(UMovieSceneSequence* Sequence)
{
	SubSequence = Sequence;

#if WITH_EDITOR
	OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
#endif
}

UMovieSceneSequence* UMovieSceneSubSection::GetSequence() const
{
	return SubSequence;
}

#if WITH_EDITOR
void UMovieSceneSubSection::PreEditChange(FProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		// Store the current subsequence in case it needs to be restored in PostEditChangeProperty because the new value would introduce a circular dependency
		PreviousSubSequence = SubSequence;
	}

	return Super::PreEditChange(PropertyAboutToChange);
}

void UMovieSceneSubSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		// Check whether the subsequence that was just set has master tracks that contain the sequence that this subsection is in.
		UMovieScene* SubSequenceMovieScene = SubSequence ? SubSequence->GetMovieScene() : nullptr;

		UMovieSceneSubTrack* TrackOuter = Cast<UMovieSceneSubTrack>(GetOuter());

		if (SubSequenceMovieScene && TrackOuter)
		{
			if (UMovieSceneSequence* CurrentSequence = TrackOuter->GetTypedOuter<UMovieSceneSequence>())
			{
				TArray<UMovieSceneSubTrack*> SubTracks;

				for (UMovieSceneTrack* MasterTrack : SubSequenceMovieScene->GetMasterTracks())
				{
					if (UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(MasterTrack))
					{
						SubTracks.Add(SubTrack);
					}
				}

				for (const FMovieSceneBinding& Binding : SubSequenceMovieScene->GetBindings())
				{
					for (UMovieSceneTrack* Track : SubSequenceMovieScene->FindTracks(UMovieSceneSubTrack::StaticClass(), Binding.GetObjectGuid()))
					{
						if (UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(Track))
						{
							SubTracks.Add(SubTrack);
						}
					}
				}

				for (UMovieSceneSubTrack* SubTrack : SubTracks)
				{
					if ( SubTrack->ContainsSequence(*CurrentSequence, true))
					{
						UE_LOG(LogMovieScene, Error, TEXT("Invalid level sequence %s. It is already contained by: %s."), *SubSequence->GetDisplayName().ToString(), *CurrentSequence->GetDisplayName().ToString());

						// Restore to the previous sub sequence because there was a circular dependency
						SubSequence = PreviousSubSequence;
						break;
					}
				}
			}
		}

		PreviousSubSequence = nullptr;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	// recreate runtime instance when sequence is changed
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
	}
}
#endif

UMovieSceneSection* UMovieSceneSubSection::SplitSection( FQualifiedFrameTime SplitTime, bool bDeleteKeys)
{
	// GetRange is in owning sequence resolution so we check against the incoming SplitTime without converting it.
	TRange<FFrameNumber> InitialRange = GetRange();
	if ( !InitialRange.Contains(SplitTime.Time.FrameNumber) )
	{
		return nullptr;
	}

	FFrameNumber InitialStartOffset = Parameters.StartFrameOffset;

	UMovieSceneSubSection* NewSection = Cast<UMovieSceneSubSection>( UMovieSceneSection::SplitSection( SplitTime, bDeleteKeys ) );
	if ( NewSection )
	{
		if (InitialRange.GetLowerBound().IsClosed())
		{
			// Sections need their offsets calculated in their local resolution. Different sequences can have different tick resolutions 
			// so we need to transform from the parent resolution to the local one before splitting them.
			FFrameRate LocalTickResolution;
			if (GetSequence())
			{
				LocalTickResolution = GetSequence()->GetMovieScene()->GetTickResolution();
			}
			else
			{
				UMovieScene* OuterScene = GetTypedOuter<UMovieScene>();
				if (OuterScene)
				{
					LocalTickResolution = OuterScene->GetTickResolution();
				}
			}

			FFrameNumber LocalResolutionStartOffset = FFrameRate::TransformTime(SplitTime.Time.GetFrame() - UE::MovieScene::DiscreteInclusiveLower(InitialRange), SplitTime.Rate, LocalTickResolution).FrameNumber;

			FFrameNumber NewStartOffset = LocalResolutionStartOffset * Parameters.TimeScale;
			NewStartOffset += InitialStartOffset;

			if (NewStartOffset >= 0)
			{
				NewSection->Parameters.StartFrameOffset = NewStartOffset.Value;
			}
		}

		return NewSection;
	}

	// Restore original offset modified by splitting
	Parameters.StartFrameOffset = InitialStartOffset;

	return nullptr;
}

TOptional<TRange<FFrameNumber> > UMovieSceneSubSection::GetAutoSizeRange() const
{
	if (SubSequence && SubSequence->GetMovieScene())
	{
		// We probably want to just auto-size the section to the sub-sequence's scaled playback range... if this section
		// is looping, however, it's hard to know what we want to do.
		FMovieSceneTimeTransform InnerToOuter = OuterToInnerTransform().InverseLinearOnly();
		UMovieScene* InnerMovieScene = SubSequence->GetMovieScene();

		FFrameTime IncAutoStartTime = FFrameTime(UE::MovieScene::DiscreteInclusiveLower(InnerMovieScene->GetPlaybackRange())) * InnerToOuter;
		FFrameTime ExcAutoEndTime   = FFrameTime(UE::MovieScene::DiscreteExclusiveUpper(InnerMovieScene->GetPlaybackRange())) * InnerToOuter;

		return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + (ExcAutoEndTime.RoundToFrame() - IncAutoStartTime.RoundToFrame()));
	}

	return Super::GetAutoSizeRange();
}

void UMovieSceneSubSection::TrimSection( FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys)
{
	TRange<FFrameNumber> InitialRange = GetRange();
	if ( !InitialRange.Contains( TrimTime.Time.GetFrame() ) )
	{
		return;
	}

	FFrameNumber InitialStartOffset = Parameters.StartFrameOffset;

	UMovieSceneSection::TrimSection( TrimTime, bTrimLeft, bDeleteKeys );

	// If trimming off the left, set the offset of the shot
	if ( bTrimLeft && InitialRange.GetLowerBound().IsClosed() && GetSequence())
	{
		// Sections need their offsets calculated in their local resolution. Different sequences can have different tick resolutions 
		// so we need to transform from the parent resolution to the local one before splitting them.
		FFrameRate LocalTickResolution = GetSequence()->GetMovieScene()->GetTickResolution();
		FFrameNumber LocalResolutionStartOffset = FFrameRate::TransformTime(TrimTime.Time.GetFrame() - UE::MovieScene::DiscreteInclusiveLower(InitialRange), TrimTime.Rate, LocalTickResolution).FrameNumber;


		FFrameNumber NewStartOffset = LocalResolutionStartOffset * Parameters.TimeScale;
		NewStartOffset += InitialStartOffset;

		// Ensure start offset is not less than 0
		if (NewStartOffset >= 0)
		{
			Parameters.StartFrameOffset = NewStartOffset;
		}
	}
}

void UMovieSceneSubSection::GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	UMovieSceneSequence* Sequence = GetSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

    TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
	const FFrameNumber StartFrame = GetInclusiveStartFrame();
	const FFrameNumber EndFrame = GetExclusiveEndFrame() - 1; // -1 because we don't need to add the end frame twice

	if (Parameters.bCanLoop)
	{
	    const float InvTimeScale = FMath::IsNearlyZero(Parameters.TimeScale) ? 1.0f : 1.0f / Parameters.TimeScale;
		const TRange<FFrameNumber> InnerPlaybackRange = UMovieSceneSubSection::GetValidatedInnerPlaybackRange(Parameters, *MovieScene);

		const FFrameNumber InnerSubSeqLength = UE::MovieScene::DiscreteSize(InnerPlaybackRange);
		const FFrameNumber InnerSubSeqFirstLoopLength = InnerSubSeqLength - Parameters.FirstLoopStartFrameOffset;

		const FFrameRate OuterFrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();
		const FFrameRate InnerFrameRate = MovieScene->GetTickResolution();
		const FFrameNumber OuterSubSeqLength = (ConvertFrameTime(InnerSubSeqLength, InnerFrameRate, OuterFrameRate) * InvTimeScale).FrameNumber;
		const FFrameNumber OuterSubSeqFirstLoopLength = (ConvertFrameTime(InnerSubSeqFirstLoopLength, InnerFrameRate, OuterFrameRate) * InvTimeScale).FrameNumber;

		FFrameNumber CurOffsetFrame = FMath::Max(OuterSubSeqFirstLoopLength, FFrameNumber(0));
	    
		while (CurOffsetFrame < EndFrame)
		{
			const int32 CurOffset = CurOffsetFrame.Value;
			      
			OutSnapTimes.Add(StartFrame + CurOffset);

			CurOffsetFrame += OuterSubSeqLength;
		}
	}
	else
	{
		const FMovieSceneSequenceTransform InnerToOuterTransform = OuterToInnerTransform().InverseLinearOnly();
		const FFrameNumber PlaybackStart = (UE::MovieScene::DiscreteInclusiveLower(PlaybackRange) * InnerToOuterTransform).FloorToFrame();
		if (GetRange().Contains(PlaybackStart))
		{
			OutSnapTimes.Add(PlaybackStart);
		}

		const FFrameNumber PlaybackEnd = (UE::MovieScene::DiscreteExclusiveUpper(PlaybackRange) * InnerToOuterTransform).FloorToFrame();
		if (GetRange().Contains(PlaybackEnd))
		{
			OutSnapTimes.Add(PlaybackEnd);
		}
	}
}

void UMovieSceneSubSection::MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	if (Parameters.StartFrameOffset.Value > 0)
	{
		FFrameNumber NewStartFrameOffset = ConvertFrameTime(FFrameTime(Parameters.StartFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Parameters.StartFrameOffset = NewStartFrameOffset;
	}
	if (Parameters.EndFrameOffset.Value > 0)
	{
		FFrameNumber NewEndFrameOffset = ConvertFrameTime(FFrameTime(Parameters.EndFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Parameters.EndFrameOffset = NewEndFrameOffset;
	}
	if (Parameters.FirstLoopStartFrameOffset.Value > 0)
	{
		FFrameNumber NewFirstLoopStartFrameOffset = ConvertFrameTime(FFrameTime(Parameters.FirstLoopStartFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		Parameters.FirstLoopStartFrameOffset = NewFirstLoopStartFrameOffset;
	}
}

FMovieSceneSubSequenceData UMovieSceneSubSection::GenerateSubSequenceData(const FSubSequenceInstanceDataParams& Params) const
{
	return FMovieSceneSubSequenceData(*this);
}

FFrameNumber UMovieSceneSubSection::MapTimeToSectionFrame(FFrameTime InPosition) const
{
	FFrameNumber LocalPosition = ((InPosition - Parameters.StartFrameOffset) * Parameters.TimeScale).GetFrame();
	return LocalPosition;
}

void UMovieSceneSubSection::BuildDefaultSubSectionComponents(UMovieSceneEntitySystemLinker* EntityLinker, const UE::MovieScene::FEntityImportParams& Params, UE::MovieScene::FImportedEntity* OutImportedEntity) const
{
	using namespace UE::MovieScene;

	if (Easing.GetEaseInDuration() > 0 || Easing.GetEaseOutDuration() > 0)
	{
		FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

		const FSubSequencePath      PathToRoot         = EntityLinker->GetInstanceRegistry()->GetInstance(Params.Sequence.InstanceHandle).GetSubSequencePath();
		const FMovieSceneSequenceID ResolvedSequenceID = PathToRoot.ResolveChildSequenceID(this->GetSequenceID());

		OutImportedEntity->AddBuilder(
			FEntityBuilder().Add(Components->HierarchicalEasingProvider, ResolvedSequenceID)
		);
	}
}


