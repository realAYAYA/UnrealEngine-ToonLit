﻿// Copyright Epic Games, Inc. All Rights Reserved.

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
	IMovieSceneConstrainedSection* InSection,
	TArray<FFrameNumber>& OutFramesToBake)
{
	const UMovieSceneSequence* MovieSceneSequence = InSequencer->GetFocusedMovieSceneSequence();
	const UMovieScene* MovieScene = MovieSceneSequence ? MovieSceneSequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	// get constraint channel data if any
	TArrayView<const FFrameNumber> ConstraintFrames;
	TArrayView<const bool> ConstraintValues;

	// note that we might want to bake a constraint which is not animated
	FConstraintAndActiveChannel* ThisActiveChannel = InSection->GetConstraintChannel(InConstraint->GetFName());
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
	FFrameNumber FirstBakingFrame = MovieScene->GetPlaybackRange().GetLowerBoundValue();
	FFrameNumber LastBakingFrame = MovieScene->GetPlaybackRange().GetUpperBoundValue();
	
	// set start to first active frame if any
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

	// then compute range from first to last
	TArray<FFrameNumber> Frames;
	MovieSceneToolHelpers::CalculateFramesBetween(MovieScene, FirstBakingFrame, LastBakingFrame, Frames);
		
	// Fill the frames we want to bake
	{
		// add constraint keys
		for (const FFrameNumber& ConstraintFrame: ConstraintFrames)
		{
			OutFramesToBake.Add(ConstraintFrame);
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
		
		for (const FFrameNumber& InFrame: Frames)
		{
			if (IsConstraintActive(InFrame))
			{
				OutFramesToBake.Add(InFrame);
			}
		}
	}

	// we also need to store which T-1 frames need to be kept for other constraints compensation
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
				if (FConstraintAndActiveChannel* ConstraintChannel = InSection->GetConstraintChannel(ConstraintName))
				{
					OtherChannels.Add(ConstraintChannel);
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

void FConstraintBaker::Bake(UWorld* InWorld, 
	UTickableTransformConstraint* InConstraint,
	const TSharedPtr<ISequencer>& InSequencer, 
	const TOptional<TArray<FFrameNumber>>& InFrames)
{
	if ((InFrames && InFrames->IsEmpty()) || (InConstraint == nullptr) || (InConstraint->ChildTRSHandle == nullptr))
	{
		return;
	}

	const TObjectPtr<UTransformableHandle>& Handle = InConstraint->ChildTRSHandle;

	const FConstraintChannelInterfaceRegistry& InterfaceRegistry = FConstraintChannelInterfaceRegistry::Get();	
	ITransformConstraintChannelInterface* Interface = InterfaceRegistry.FindConstraintChannelInterface(Handle->GetClass());
	if (!Interface)
	{
		return;
	}
	
	//get the section to be used later to delete the extra transform keys at the frame -1 times, abort if not there for some reason
	UMovieSceneSection* ConstraintSection = Interface->GetHandleConstraintSection(Handle, InSequencer);
	const UMovieSceneSection* TransformSection = Interface->GetHandleSection(Handle, InSequencer);

	IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(ConstraintSection);
	if (ConstrainedSection == nullptr || TransformSection == nullptr)
	{
		return;
	}
	
	FConstraintAndActiveChannel* ActiveChannel = ConstrainedSection->GetConstraintChannel(InConstraint->GetFName());
	if (ActiveChannel == nullptr)
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
		GetMinimalFramesToBake(InWorld, InConstraint, InSequencer, ConstrainedSection, FramesToBake);

		// if it needs recomposition 
		if (ConstraintSection != TransformSection && !FramesToBake.IsEmpty())
		{
			const TMovieSceneChannelData<const bool> ConstraintChannelData = ActiveChannel->ActiveChannel.GetData();
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
	
	FCompensationEvaluator Evaluator(InConstraint);
	Evaluator.ComputeLocalTransformsForBaking(InWorld, InSequencer, FramesToBake);
	const TArray<FTransform>& Transforms = Evaluator.ChildLocals;
	if (FramesToBake.Num() != Transforms.Num())
	{
		return;
	}
	
	ConstraintSection->Modify();

	// disable constraint and delete extra transform keys
	TMovieSceneChannelData<bool> ConstraintChannelData = ActiveChannel->ActiveChannel.GetData();
	const TArrayView<const FFrameNumber> ConstraintFrames = ConstraintChannelData.GetTimes();
	
	// get transform channels
	const TArrayView<FMovieSceneFloatChannel*> FloatTransformChannels = Handle->GetFloatChannels(TransformSection);
	const TArrayView<FMovieSceneDoubleChannel*> DoubleTransformChannels = Handle->GetDoubleChannels(TransformSection);

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
			}
			else if (DoubleTransformChannels.Num() > 0)
			{
				FMovieSceneConstraintChannelHelper::DeleteTransformKeys(DoubleTransformChannels, FrameMinusOne);
			}
		}
	}

	// now bake to channel curves
	const EMovieSceneTransformChannel Channels = InConstraint->GetChannelsToKey();
	Interface->AddHandleTransformKeys(InSequencer, Handle, FramesToBake, Transforms, Channels);

	// notify
	InSequencer->RequestEvaluate();
}

#undef LOCTEXT_NAMESPACE


