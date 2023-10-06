// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ConstraintBaker.h"

#include "TransformConstraint.h"
#include "TransformableHandle.h"
#include "ConstraintChannel.h"

#include "ISequencer.h"

#include "LevelSequence.h"
#include "MovieSceneToolHelpers.h"

#include "Constraints/MovieSceneConstraintChannelHelper.inl"
#include "Constraints/TransformConstraintChannelInterface.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"

#define LOCTEXT_NAMESPACE "ConstraintBaker"


// at this stage, we suppose that any of these arguments are safe to use. Make sure to test them before using that function
void FConstraintBaker::GetMinimalFramesToBake(
	UWorld* InWorld,
	const UTickableTransformConstraint* InConstraint,
	const TSharedPtr<ISequencer>& InSequencer,
	UMovieSceneSection* InSection,
	const TOptional< FBakingAnimationKeySettings>& InSettings,
	TArray<FFrameNumber>& OutFramesToBake)
{
	const UMovieSceneSequence* MovieSceneSequence = InSequencer->GetFocusedMovieSceneSequence();
	const UMovieScene* MovieScene = MovieSceneSequence ? MovieSceneSequence->GetMovieScene() : nullptr;
	if (!MovieScene || !InConstraint || !InConstraint->ChildTRSHandle || !InSection)
	{
		return;
	}
	IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(InSection);

	// get constraint channel data if any
	TArrayView<const FFrameNumber> ConstraintFrames;
	TArrayView<const bool> ConstraintValues;

	// note that we might want to bake a constraint which is not animated
	FConstraintAndActiveChannel* ThisActiveChannel = ConstrainedSection->GetConstraintChannel(InConstraint->GetFName());
	if (ThisActiveChannel)
	{
		const TMovieSceneChannelData<const bool> ConstraintChannelData = ThisActiveChannel->ActiveChannel.GetData();
		ConstraintFrames = ConstraintChannelData.GetTimes();
		ConstraintValues = ConstraintChannelData.GetValues();
	}

	// there's no channel or an empty one and the constraint is inactive so no need to do anything if the constraint
	// is not active
	if (ConstraintFrames.IsEmpty() && !InConstraint->IsFullyActive())
	{
		return;
	}

	// default init bounds to the scene bounds
	FFrameNumber FirstBakingFrame = InSettings.IsSet() ? InSettings.GetValue().StartFrame : MovieScene->GetPlaybackRange().GetLowerBoundValue();
	FFrameNumber LastBakingFrame = InSettings.IsSet() ? InSettings.GetValue().EndFrame : MovieScene->GetPlaybackRange().GetUpperBoundValue();
	
	// set start to first active frame if any, if we have no range
	if (InSettings.IsSet() == false)
	{
		const int32 FirstActiveIndex = ConstraintValues.IndexOfByKey(true);
		if (ConstraintValues.IsValidIndex(FirstActiveIndex))
		{
			FirstBakingFrame = ConstraintFrames[FirstActiveIndex];
		}

		// set end to the last key if inactive
		const bool bIsLastKeyInactive = (ConstraintValues.Last() == false);
		if (bIsLastKeyInactive)
		{
			LastBakingFrame = ConstraintFrames.Last();
		}
	}
	// keep frames within active state
	auto IsConstraintActive = [InConstraint, ThisActiveChannel](const FFrameNumber& Time)
	{
		if (ThisActiveChannel)
		{
			bool bIsActive = false; ThisActiveChannel->ActiveChannel.Evaluate(Time, bIsActive);
			return bIsActive;
		}
		return InConstraint->IsFullyActive();
	};

	if (InSettings.IsSet() == false || InSettings.GetValue().BakingKeySettings != EBakingKeySettings::KeysOnly)
	{
		TArray<FFrameNumber> Frames;
		MovieSceneToolHelpers::CalculateFramesBetween(MovieScene, FirstBakingFrame, LastBakingFrame,  1 /* use increment of 1 since we reduce later*/,
			Frames);

		// Fill the frames we want to bake

		// add constraint keys
		for (const FFrameNumber& ConstraintFrame : ConstraintFrames)
		{
			OutFramesToBake.Add(ConstraintFrame);
		}

		int32 FrameInc = (InSettings.IsSet() == true) ? InSettings.GetValue().FrameIncrement : 1;
		for (int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex)
		{
			if (FrameIndex % FrameInc == 0)
			{
				const FFrameNumber& InFrame = Frames[FrameIndex];
				if (IsConstraintActive(InFrame))
				{
					OutFramesToBake.Add(InFrame);
				}
			}
		}
		if (FrameInc > 1) //if skipping make sure we get all transforms
		{
			FMovieSceneConstraintChannelHelper::GetTransformFramesForConstraintHandles(InConstraint, InSequencer,
				InSettings.GetValue().StartFrame, InSettings.GetValue().EndFrame, OutFramesToBake);
		}
		
	}
	else //doing per frame key get constraint keys and transform keys
	{
		for (const FFrameNumber& Frame : ConstraintFrames)
		{
			if (Frame >= FirstBakingFrame && Frame <= LastBakingFrame)
			{
				OutFramesToBake.Add(Frame);
			}
		}

		FMovieSceneConstraintChannelHelper::GetTransformFramesForConstraintHandles(InConstraint, InSequencer,
			InSettings.GetValue().StartFrame, InSettings.GetValue().EndFrame, OutFramesToBake);

	}

	// we also need to store which T-1 frames need to be kept for other constraints compensation
	// and also store transform key locations for parents if doing per key or per frame with non-1 increments
	{
		// gather the sorted child's constraint
		static constexpr bool bSorted = true;
		const uint32 ChildHash = InConstraint->ChildTRSHandle->GetHash();
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
		using ConstraintPtr = TObjectPtr<UTickableConstraint>;
		const TArray<ConstraintPtr> Constraints = Controller.GetParentConstraints(ChildHash, bSorted);

		// store the other channels that may need compensation
		TArray<FConstraintAndActiveChannel*> OtherChannels;
		for (const ConstraintPtr& Constraint: Constraints)
		{
			const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint.Get());
			if (TransformConstraint && TransformConstraint->NeedsCompensation() && TransformConstraint != InConstraint) 
			{
				const FName ConstraintName = TransformConstraint->GetFName();
				if (FConstraintAndActiveChannel* ConstraintChannel = ConstrainedSection->GetConstraintChannel(ConstraintName))
				{
					OtherChannels.Add(ConstraintChannel);
				}

				//if doing keys or per frame increments we need to also 
				if (InSettings.IsSet() == true &&
					(InSettings.GetValue().BakingKeySettings == EBakingKeySettings::KeysOnly || InSettings.GetValue().FrameIncrement > 1))
				{
					FMovieSceneConstraintChannelHelper::GetTransformFramesForConstraintHandles(TransformConstraint, InSequencer,
						InSettings.GetValue().StartFrame, InSettings.GetValue().EndFrame, OutFramesToBake);
				}
			}
		}

		// check if any other channel needs to compensate at T
		for (const FFrameNumber& ConstraintFrame: ConstraintFrames)
		{
			const bool bNeedsCompensation = OtherChannels.ContainsByPredicate(
		[ConstraintFrame](const FConstraintAndActiveChannel* OtherChannel)
			{
				const bool bHasAtLeastOneActiveKey = OtherChannel->ActiveChannel.GetValues().Contains(true);
				if (!bHasAtLeastOneActiveKey)
				{
					return false;
				}
				return OtherChannel->ActiveChannel.GetTimes().Contains(ConstraintFrame);
			} );

			// add T-1 if needed so that we will compensate int the remaining constraints' space 
			if (bNeedsCompensation)
			{
				const FFrameNumber& FrameMinusOne(ConstraintFrame-1);
				OutFramesToBake.Add(FrameMinusOne);
			}
		}
	}
	
	// uniqueness
	OutFramesToBake.Sort();
	OutFramesToBake.SetNum(Algo::Unique(OutFramesToBake));
}

bool FConstraintBaker::BakeMultiple(
	UWorld* InWorld,
	TArray< UTickableTransformConstraint*>& InConstraints,
	const TSharedPtr<ISequencer>& InSequencer,
	const FBakingAnimationKeySettings& InSettings)
{
	TOptional<TArray<FFrameNumber>> Frames;
	TOptional< FBakingAnimationKeySettings> Settings = InSettings;
	for (UTickableTransformConstraint* Constraint : InConstraints)
	{
		if (IsValid(Constraint->ChildTRSHandle) && Constraint->ChildTRSHandle->IsValid()
			&& IsValid(Constraint->ParentTRSHandle) && Constraint->ParentTRSHandle->IsValid())
		{
			FConstraintBaker::Bake(InWorld, Constraint, InSequencer, Settings, Frames);
		}
	}
	return true;
}

void FConstraintBaker::Bake(UWorld* InWorld, 
	UTickableTransformConstraint* InConstraint,
	const TSharedPtr<ISequencer>& InSequencer, 
	const TOptional< FBakingAnimationKeySettings>& InSettings,
	const TOptional<TArray<FFrameNumber>>& InFrames)
{
	if ((InFrames && InFrames->IsEmpty()) || (InConstraint == nullptr) || (InConstraint->ChildTRSHandle == nullptr))
	{
		return;
	}

	const TObjectPtr<UTransformableHandle>& Handle = InConstraint->ChildTRSHandle;

	FConstraintSections ConstraintSections = FMovieSceneConstraintChannelHelper::GetConstraintSectionAndChannel(
		InConstraint, InSequencer);
	IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(ConstraintSections.ConstraintSection);
	if (ConstrainedSection == nullptr) 
	{
		return;
	}
	// compute transforms
	TArray<FFrameNumber> FramesToBake;
	if (InFrames)
	{
		FramesToBake = *InFrames;
	}
	else
	{
		GetMinimalFramesToBake(InWorld, InConstraint, InSequencer, ConstraintSections.ConstraintSection, InSettings, FramesToBake);

		// if it needs recomposition 
		if (ConstraintSections.ConstraintSection != ConstraintSections.ChildTransformSection && !FramesToBake.IsEmpty())
		{
			const TMovieSceneChannelData<const bool> ConstraintChannelData = ConstraintSections.ActiveChannel->ActiveChannel.GetData();
			const TArrayView<const FFrameNumber> ConstraintFrames = ConstraintChannelData.GetTimes();
			const TArrayView<const bool> ConstraintValues = ConstraintChannelData.GetValues();
			for (int32 Index = 0; Index < ConstraintFrames.Num(); ++Index)
			{
				// if the key is active
				if (ConstraintValues[Index])
				{
					const int32 FrameIndex = FramesToBake.IndexOfByKey(ConstraintFrames[Index]);
					if (FrameIndex != INDEX_NONE)
					{
						// T-1 is not part of the frames to bake, then we need to compensate:
						// in the context of additive, we need to add a 'zeroed' key to prevent from popping
						const FFrameNumber FrameMinusOne(ConstraintFrames[Index] - 1);
						const bool bAddMinusOne = (FrameIndex == 0) ? true : !ConstraintFrames.Contains(FrameMinusOne);  
						if (bAddMinusOne)
						{
							FramesToBake.Insert(FrameMinusOne, FrameIndex);
						}
					}
				}
			}
		}
	}
	if (FramesToBake.Num() == 0 || ConstraintSections.ConstraintSection == nullptr ||
		ConstraintSections.ActiveChannel == nullptr)
	{
		return;
	}

	FCompensationEvaluator Evaluator(InConstraint);
	Evaluator.ComputeLocalTransformsForBaking(InWorld, InSequencer, FramesToBake);
	TArray<FTransform> Transforms = Evaluator.ChildLocals;
	if (FramesToBake.Num() != Transforms.Num())
	{
		return;
	}
	
	ConstraintSections.ConstraintSection->Modify();

	// disable constraint and delete extra transform keys
	TMovieSceneChannelData<bool> ConstraintChannelData = ConstraintSections.ActiveChannel->ActiveChannel.GetData();
	const TArrayView<const FFrameNumber> ConstraintFrames = ConstraintChannelData.GetTimes();
	
	// get transform channels
	const TArrayView<FMovieSceneFloatChannel*> FloatTransformChannels = Handle->GetFloatChannels(ConstraintSections.ChildTransformSection);
	const TArrayView<FMovieSceneDoubleChannel*> DoubleTransformChannels = Handle->GetDoubleChannels(ConstraintSections.ChildTransformSection);

	EMovieSceneKeyInterpolation KeyType = InSequencer->GetKeyInterpolation();

	for (int32 Index = 0; Index < ConstraintFrames.Num(); ++Index)
	{
		const FFrameNumber Frame = ConstraintFrames[Index];
		//todo we need to add a key at the begin/end if there is no frame there
		if (Frame >= FramesToBake[0] && Frame <= FramesToBake.Last())
		{
			// set constraint key to inactive
			ConstraintChannelData.UpdateOrAddKey(Frame, false);

			// delete minus one transform frames
			const FFrameNumber FrameMinusOne = Frame - 1;
			if (FloatTransformChannels.Num() > 0)
			{
				FMovieSceneConstraintChannelHelper::DeleteTransformKeys(FloatTransformChannels, FrameMinusOne);
				//we also set the tangent at the break to the default type
				FMovieSceneConstraintChannelHelper::ChangeKeyInterpolation(FloatTransformChannels, Frame, KeyType);
			}
			else if (DoubleTransformChannels.Num() > 0)
			{
				FMovieSceneConstraintChannelHelper::DeleteTransformKeys(DoubleTransformChannels, FrameMinusOne);
				//we also set the tangent at the break to the default type
				FMovieSceneConstraintChannelHelper::ChangeKeyInterpolation(DoubleTransformChannels, Frame, KeyType);
			}
			if (const int32 RemoveIndex = Algo::BinarySearch(FramesToBake, FrameMinusOne);
				RemoveIndex != INDEX_NONE)
			{
				FramesToBake.RemoveAt(RemoveIndex);
				Transforms.RemoveAt(RemoveIndex);
			}
			
		}
	}
	//remove transform keys if baking frame
	const EMovieSceneTransformChannel ChannelsToKey = InConstraint->GetChannelsToKey();

	if (InSettings.IsSet() && InSettings.GetValue().BakingKeySettings == EBakingKeySettings::AllFrames)
	{
		if (FloatTransformChannels.Num() > 0)
		{
			FMovieSceneConstraintChannelHelper::DeleteTransformTimes(FloatTransformChannels, FramesToBake[0] +1, FramesToBake[FramesToBake.Num() -1],
				ChannelsToKey);
		}
		else if (DoubleTransformChannels.Num() > 0)
		{
			FMovieSceneConstraintChannelHelper::DeleteTransformTimes(DoubleTransformChannels, FramesToBake[0] + 1, FramesToBake[FramesToBake.Num() - 1],
				ChannelsToKey);
		}
	}

	// now bake to channel curves
	const EMovieSceneTransformChannel Channels = InConstraint->GetChannelsToKey();
	ConstraintSections.Interface->AddHandleTransformKeys(InSequencer, Handle, FramesToBake, Transforms, Channels);

	if (InSettings.IsSet() && InSettings.GetValue().BakingKeySettings != EBakingKeySettings::KeysOnly
		&& InSettings.GetValue().bReduceKeys == true)
	{
		FKeyDataOptimizationParams Param;
		Param.bAutoSetInterpolation = true;
		Param.Tolerance = InSettings.GetValue().Tolerance;
		TRange<FFrameNumber> Range(InSettings.GetValue().StartFrame, InSettings.GetValue().EndFrame);
		Param.Range = Range;
		MovieSceneToolHelpers::OptimizeSection(Param, ConstraintSections.ConstraintSection);
	}
	// notify
	InSequencer->RequestEvaluate();
}

#undef LOCTEXT_NAMESPACE


