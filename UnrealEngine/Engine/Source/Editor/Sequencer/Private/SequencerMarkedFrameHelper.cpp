// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerMarkedFrameHelper.h"

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieSceneSectionParameters.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/MovieSceneTimeArray.h"
#include "Evaluation/MovieSceneTimeTransform.h"
#include "HAL/PlatformCrt.h"
#include "ISequencer.h"
#include "Math/Range.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"
#include "MovieScene.h"
#include "MovieSceneFrameMigration.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneTimeHelpers.h"
#include "Sections/MovieSceneSubSection.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"

namespace UE
{
namespace MovieScene
{

static void FindGlobalMarkedFrames(
		const ISequencer& Sequencer, const FMovieSceneSequenceHierarchy* SequenceHierarchy,
		FMovieSceneSequenceIDRef FocusedSequenceID, FMovieSceneSequenceIDRef SequenceID, 
		TRange<FFrameNumber> GatherRange,
		TMovieSceneTimeArray<FMovieSceneMarkedFrame>& OutTimestampedGlobalMarkedFrames)
{
	// Find the current sequence in the hierarchy.
	const FMovieSceneSubSequenceData* const SequenceSubData = SequenceHierarchy->FindSubData(SequenceID);
	const UMovieSceneSequence* const Sequence = SequenceSubData ? SequenceSubData->GetSequence() : Sequencer.GetRootMovieSceneSequence();
	const UMovieScene* const MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (UNLIKELY(!ensure(MovieScene)))
	{
		return;
	}

	// Get the marked frames of the current sequence if it's not the focused sequence.
	if (SequenceID != FocusedSequenceID && MovieScene->GetGloballyShowMarkedFrames())
	{
		const TArray<FMovieSceneMarkedFrame>& MarkedFrames = MovieScene->GetMarkedFrames();
		for (const FMovieSceneMarkedFrame& MarkedFrame : MarkedFrames)
		{
			if (GatherRange.Contains(MarkedFrame.FrameNumber))
			{
				OutTimestampedGlobalMarkedFrames.Add(MarkedFrame.FrameNumber, MarkedFrame);
			}
		}
	}

	// Dive into the current sequence's sub-sequences.
	const FMovieSceneSequenceHierarchyNode* const SequenceNode = SequenceHierarchy->FindNode(SequenceID);
	if (ensure(SequenceNode))
	{
		for (const FMovieSceneSequenceID ChildID : SequenceNode->Children)
		{
			const FMovieSceneSubSequenceData* const ChildSubData = SequenceHierarchy->FindSubData(ChildID);
			if (UNLIKELY(!ensure(ChildSubData)))
			{
				continue;
			}

			const UMovieSceneSequence* const ChildSequence = ChildSubData->GetSequence();
			const UMovieScene* const ChildMovieScene = ChildSequence ? ChildSequence->GetMovieScene() : nullptr;
			if (UNLIKELY(!ensure(ChildMovieScene)))
			{
				continue;
			}

			if (!ChildSubData->bCanLoop || FMath::IsNearlyZero(ChildSubData->OuterToInnerTransform.GetTimeScale()))
			{
				// This child doesn't loop (or it does, but we have zero timescale, so it's irrelevant). We just need to take into account its offset and time scale, and how much of it we "see" through the
				// parent section.
				ensure(!ChildSubData->OuterToInnerTransform.IsLooping() || FMath::IsNearlyZero(ChildSubData->OuterToInnerTransform.GetTimeScale()));

				const FMovieSceneTimeTransform& OuterToInnerTransform(ChildSubData->OuterToInnerTransform.LinearTransform);
				OutTimestampedGlobalMarkedFrames.PushTransform(OuterToInnerTransform);

				// We may not be able to loop, but we may have multiple nested transforms if we have a zero timescale
				const int NumNestedTransforms = ChildSubData->OuterToInnerTransform.NestedTransforms.Num();
				for (int i = 0; i < NumNestedTransforms; ++i)
				{
					const FMovieSceneTimeTransform& SubOuterToInnerTransform(ChildSubData->OuterToInnerTransform.NestedTransforms[i].LinearTransform);
					OutTimestampedGlobalMarkedFrames.PushTransform(SubOuterToInnerTransform);
				}

				// Compute the "window" of the child that we can see through the parent section. Note that the parent section could extend past
				// the end of the child's playback... we don't trim that, so that any marked frame added after the playback end in the child sequence
				// is still visible in the parent sequence as long as the parent section is long enough. This lets artists see what they're "missing".
				const TRange<FFrameNumber> ParentPlayRange = ChildSubData->ParentPlayRange.Value;
				const TRange<FFrameNumber> ChildGatherRange = ChildSubData->OuterToInnerTransform.TransformRangeConstrained(ParentPlayRange);

				// Gather marked frames in this "window".
				FindGlobalMarkedFrames(Sequencer, SequenceHierarchy, FocusedSequenceID, ChildID, ChildGatherRange, OutTimestampedGlobalMarkedFrames);
				
				// Pop the nested transforms off, including the base linear transform
				for (int i = 0; i < NumNestedTransforms + 1; ++i)
				{
					OutTimestampedGlobalMarkedFrames.PopTransform();
				}
			}
			else if (ensure(ChildSubData->OuterToInnerTransform.NestedTransforms.Num() > 0))
			{
				// This child is looping. Things are... more complicated.
				//
				// First, we push the time transform for this child sequence. It should be found in the first NestedTransforms
				// since that's how sub-datas are computed.
				ensure(ChildSubData->OuterToInnerTransform.LinearTransform.IsIdentity());
				const FMovieSceneNestedSequenceTransform& OuterToInnerTransform(ChildSubData->OuterToInnerTransform.NestedTransforms[0]);
				OutTimestampedGlobalMarkedFrames.PushTransform(OuterToInnerTransform.LinearTransform, OuterToInnerTransform.Warping);

				// Next, we'll need to gather the marked frames of the child sequence repeatedly, once for each loop.
				// To know how many loops we have, we need to look at the play range of the parent sub-section, so let's grab
				// that first.
				const FMovieSceneSectionParameters SubSectionParameters = ChildSubData->ToSubSectionParameters();
				const TRange<FFrameNumber> ChildPlayRange = UMovieSceneSubSection::GetValidatedInnerPlaybackRange(SubSectionParameters, *ChildMovieScene);
				const FFrameNumber ChildLength = UE::MovieScene::DiscreteSize(ChildPlayRange);

				// Now we need to know how long this child play range is in the parent's time space. This is how we can figure
				// out how many loops we can fit.
				const FFrameRate ParentFrameRate = MovieScene->GetTickResolution();
				const FFrameRate ChildFrameRate   = ChildMovieScene->GetTickResolution();

				const float ChildTimeScale = OuterToInnerTransform.LinearTransform.TimeScale;
				const float InvChildTimeScale = FMath::IsNearlyZero(ChildTimeScale) ? 1.0f : 1.0f / ChildTimeScale;

				const FFrameNumber ChildLengthInParentSpace = (ConvertFrameTime(ChildLength, ChildFrameRate, ParentFrameRate) * InvChildTimeScale).FrameNumber;
				const FFrameNumber ChildFirstLoopLength = ChildLength - ChildSubData->ParentFirstLoopStartFrameOffset;
				const FFrameNumber ChildFirstLoopLengthInParentSpace = (ConvertFrameTime(ChildFirstLoopLength, ChildFrameRate, ParentFrameRate) * InvChildTimeScale).FrameNumber;

				// We can finally start iterating: we iterate for how many times as we can fit the child sequence's length,
				// modified by the time scale, into the parent play range.
				const TRange<FFrameNumber> ParentPlayRange = ChildSubData->ParentPlayRange.Value;
				const FFrameNumber ParentExclusiveEnd = UE::MovieScene::DiscreteExclusiveUpper(ParentPlayRange);
				FFrameNumber CurLoopStart = UE::MovieScene::DiscreteInclusiveLower(ParentPlayRange);
				FFrameNumber CurLoopEnd = CurLoopStart + FMath::Min(
						FMath::Max(ChildFirstLoopLengthInParentSpace, FFrameNumber(0)),
						ParentExclusiveEnd);

				while (CurLoopStart < ParentExclusiveEnd)
				{
					TRange<FFrameNumber> CurLoopChildGatherRange(CurLoopStart, CurLoopEnd);
					CurLoopChildGatherRange = ChildSubData->OuterToInnerTransform.TransformRangeConstrained(CurLoopChildGatherRange);

					FindGlobalMarkedFrames(Sequencer, SequenceHierarchy, FocusedSequenceID, ChildID, CurLoopChildGatherRange, OutTimestampedGlobalMarkedFrames);

					CurLoopStart = CurLoopEnd;
					CurLoopEnd += FMath::Max(ChildLengthInParentSpace, FFrameNumber(0));
					
					OutTimestampedGlobalMarkedFrames.IncrementWarpCounter();
				}

				OutTimestampedGlobalMarkedFrames.PopTransform();
			}
		}
	}
}

} // namespace MovieScene
} // namespace UE

void FSequencerMarkedFrameHelper::FindGlobalMarkedFrames(ISequencer& Sequencer, TArray<uint32> LoopCounter, TArray<FMovieSceneMarkedFrame>& OutGlobalMarkedFrames)
{
	// Get the focused sequence info. We want to gather all the marked frames that are in the subset of the sequence hierarchy
	// that hangs below this focused sequence.
	UMovieSceneSequence* FocusedMovieSequence = Sequencer.GetFocusedMovieSceneSequence();
	const FMovieSceneSequenceID FocusedMovieSequenceID = Sequencer.GetFocusedTemplateID();

	UMovieSceneSequence* RootMovieSequence = Sequencer.GetRootMovieSceneSequence();

	if (!FocusedMovieSequence || !RootMovieSequence)
	{
		return;
	}

	// Get the sequence hierarchy so that we can iterate it.
	const FMovieSceneRootEvaluationTemplateInstance& EvalTemplate = Sequencer.GetEvaluationTemplate();
	const FMovieSceneSequenceHierarchy* SequenceHierarchy = EvalTemplate.GetHierarchy();
	if (!SequenceHierarchy)
	{
		return;
	}
	
	// All the marked frames will be added using their root time, but we want to actually display them in the time space of whatever
	// is the currently focused sequence. We therefore add the inverse time transform of the focused sequence at the top of the
	// transform stack if the focused sequence isn't the root sequence (which has no time transform).

	TMovieSceneTimeArray<FMovieSceneMarkedFrame> TimestampedGlobalMarkedFrames;
	const FMovieSceneSubSequenceData* FocusedMovieSequenceSubData = SequenceHierarchy->FindSubData(FocusedMovieSequenceID);
	if (FocusedMovieSequenceSubData)
	{
		// If we have a warping, but not looping transform, we need to do the inverse in pieces rather than just a single transform, 
		// because if we have zero-timescale anywhere in the mix, then we lose information if we just take a simple FMovieSceneTimeTransform as an inverse.
		if (FocusedMovieSequenceSubData->RootToSequenceTransform.IsWarping() && !FocusedMovieSequenceSubData->RootToSequenceTransform.IsLooping())
		{
			const int NumNestedTransforms = FocusedMovieSequenceSubData->RootToSequenceTransform.NestedTransforms.Num();
			for (int i = NumNestedTransforms - 1; i >= 0; --i)
			{
				const FMovieSceneTimeTransform& SubOuterToInnerTransform(FocusedMovieSequenceSubData->RootToSequenceTransform.NestedTransforms[i].LinearTransform.Inverse());
				TimestampedGlobalMarkedFrames.PushTransform(SubOuterToInnerTransform);
			}
			TimestampedGlobalMarkedFrames.PushTransform(FocusedMovieSequenceSubData->RootToSequenceTransform.LinearTransform.Inverse());
		}
		else
		{
			const int32 FocusedMovieSequenceDepth = FMath::Min(
				FocusedMovieSequenceSubData->RootToSequenceTransform.NestedTransforms.Num(),
				LoopCounter.Num());
			TArrayView<uint32> LoopCounterForFocusedMovieSequence = MakeArrayView(LoopCounter.GetData(), FocusedMovieSequenceDepth);
			FMovieSceneSequenceTransform InverseLoopTransform = FocusedMovieSequenceSubData->RootToSequenceTransform.InverseFromLoop(LoopCounterForFocusedMovieSequence);

			TimestampedGlobalMarkedFrames.PushTransform(InverseLoopTransform.LinearTransform);
			for (int i = 0; i < InverseLoopTransform.NestedTransforms.Num(); ++i)
			{
				TimestampedGlobalMarkedFrames.PushTransform(InverseLoopTransform.NestedTransforms[i].LinearTransform);
			}
		}
	}

	// Grab the marked frames from the root sequence, and recursively across the whole hierarchy.
	UE::MovieScene::FindGlobalMarkedFrames(Sequencer, SequenceHierarchy, FocusedMovieSequenceID, MovieSceneSequenceID::Root, TRange<FFrameNumber>::All(), TimestampedGlobalMarkedFrames);

	// Export the modified timestamped entries.
	for (const TMovieSceneTimeArrayEntry<FMovieSceneMarkedFrame>& Entry : TimestampedGlobalMarkedFrames.GetEntries())
	{
		FMovieSceneMarkedFrame MarkedFrame = Entry.Datum;
		MarkedFrame.FrameNumber = Entry.RootTime.FrameNumber;
		OutGlobalMarkedFrames.Add(MarkedFrame);
	}
}

void FSequencerMarkedFrameHelper::ClearGlobalMarkedFrames(ISequencer& Sequencer)
{
	const FMovieSceneRootEvaluationTemplateInstance& EvalTemplate = Sequencer.GetEvaluationTemplate();

	ClearGlobalMarkedFrames(EvalTemplate.GetRootSequence());

	const FMovieSceneSequenceHierarchy* SequenceHierarchy = EvalTemplate.GetHierarchy();
	if (SequenceHierarchy)
	{
		for (const TTuple<FMovieSceneSequenceID, FMovieSceneSubSequenceData>& Pair : SequenceHierarchy->AllSubSequenceData())
		{
			ClearGlobalMarkedFrames(Pair.Value.GetSequence());
		}
	}
}

void FSequencerMarkedFrameHelper::ClearGlobalMarkedFrames(UMovieSceneSequence* Sequence)
{
	if (Sequence)
	{
		if (UMovieScene* MovieScene = Sequence->GetMovieScene())
		{
			MovieScene->SetGloballyShowMarkedFrames(false);
		}
	}
}

