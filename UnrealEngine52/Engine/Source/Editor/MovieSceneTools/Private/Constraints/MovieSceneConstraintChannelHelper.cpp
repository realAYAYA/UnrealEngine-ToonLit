﻿// Copyright Epic Games, Inc. All Rights Reserved.

#include "Constraints/MovieSceneConstraintChannelHelper.inl"

#include "ISequencer.h"

#include "TransformableHandle.h"
#include "LevelSequence.h"
#include "MovieSceneToolHelpers.h"

#include "TransformConstraint.h"
#include "Algo/Copy.h"

#include "ScopedTransaction.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "LevelEditorViewport.h"
#include "MovieSceneSpawnableAnnotation.h"
#include "Sections/MovieSceneConstrainedSection.h"
#include "MovieSceneToolsModule.h"
#include "Constraints/TransformConstraintChannelInterface.h"


/*
*
*  FCompensationEvaluator
*
*/

FCompensationEvaluator::FCompensationEvaluator(UTickableTransformConstraint* InConstraint)
	: Constraint(InConstraint)
	, Handle(InConstraint ? InConstraint->ChildTRSHandle : nullptr)
{}

void FCompensationEvaluator::ComputeLocalTransforms(
	UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer,
	const TArray<FFrameNumber>& InFrames, const bool bToActive)
{
	if (InFrames.IsEmpty())
	{
		return;
	}

	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	const TArray< ConstraintPtr > Constraints = GetHandleTransformConstraints(InWorld);
	if (Constraints.IsEmpty())
	{
		return;
	}
	
	const TArray<ConstraintPtr> ConstraintsMinusThis =
		Constraints.FilterByPredicate([this](const ConstraintPtr& InConstraint)
		{
			return InConstraint != Constraint;
		});

	// find last active constraint in the list that is different than the on we want to compensate for
	auto GetLastActiveConstraint = [ConstraintsMinusThis]()
	{
		// find last active constraint in the list that is different than the one we want to compensate for
		const int32 LastActiveIndex = FTransformConstraintUtils::GetLastActiveConstraintIndex(ConstraintsMinusThis);

		// if found, return its parent global transform
		return LastActiveIndex > INDEX_NONE ? Cast<UTickableTransformConstraint>(ConstraintsMinusThis[LastActiveIndex]) : nullptr;
	};

	UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const EMovieScenePlayerStatus::Type PlaybackStatus = InSequencer->GetPlaybackStatus();

	const int32 NumFrames = InFrames.Num();

	// resize arrays to num frames + 1 as we also evaluate at InFrames[0]-1
	ChildLocals.SetNum(NumFrames + 1);
	ChildGlobals.SetNum(NumFrames + 1);
	SpaceGlobals.SetNum(NumFrames + 1);
	const TArray<IMovieSceneToolsAnimationBakeHelper*>& BakeHelpers = FMovieSceneToolsModule::Get().GetAnimationBakeHelpers();
	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StartBaking(MovieScene);
		}
	}

	// get all constraints to evaluate
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	static constexpr bool bSorted = true;
	const TArray<ConstraintPtr> AllConstraints = Controller.GetAllConstraints(bSorted);
	FMovieSceneSequenceTransform RootToLocalTransform = InSequencer->GetFocusedMovieSceneSequenceTransform();

	for (int32 Index = 0; Index < NumFrames + 1; ++Index)
	{
		FFrameNumber FrameNumber = (Index == 0) ? InFrames[0] - 1 : InFrames[Index - 1];
		FrameNumber = (FFrameTime(FrameNumber) * RootToLocalTransform.InverseLinearOnly()).GetFrame();
		// evaluate animation
		const FMovieSceneEvaluationRange EvaluationRange = FMovieSceneEvaluationRange(FFrameTime(FrameNumber), TickResolution);
		const FMovieSceneContext Context = FMovieSceneContext(EvaluationRange, PlaybackStatus).SetHasJumped(true);

		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PreEvaluation(MovieScene, FrameNumber);
			}
		}
		InSequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *InSequencer);

		// evaluate constraints
		for (const UTickableConstraint* InConstraint : AllConstraints)
		{
			InConstraint->Evaluate();
		}

		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PostEvaluation(MovieScene, FrameNumber);
			}
		}
		// evaluate ControlRig?
		// ControlRig->Evaluate_AnyThread();

		FTransform& ChildLocal = ChildLocals[Index];
		FTransform& ChildGlobal = ChildGlobals[Index];
		FTransform& SpaceGlobal = SpaceGlobals[Index];

		// store child transforms        	
		ChildLocal = Handle->GetLocalTransform();
		ChildGlobal = Handle->GetGlobalTransform();

		const UTickableTransformConstraint* LastConstraint = GetLastActiveConstraint();

		// store constraint/parent space global transform
		if (bToActive)
		{
			// if activating the constraint, store last constraint or parent space at T[0]-1
			// and constraint space for all other times
			if (Index == 0)
			{
				if (LastConstraint)
				{
					SpaceGlobal = LastConstraint->GetParentGlobalTransform();
					TOptional<FTransform> Relative =
						FTransformConstraintUtils::GetConstraintsRelativeTransform(ConstraintsMinusThis, ChildLocal, ChildGlobal);
					if (Relative)
					{
						ChildLocal = *Relative;
					}
				}
			}
			else
			{
				SpaceGlobal = Constraint->GetParentGlobalTransform();
				ChildLocal = FTransformConstraintUtils::ComputeRelativeTransform(
					ChildLocal, ChildGlobal, SpaceGlobal, Constraint);
			}
		}
		else
		{
			// if deactivating the constraint, store constraint space at T[0]-1
			// and last constraint or parent space for all other times
			if (Index == 0)
			{
				SpaceGlobal = Constraint->GetParentGlobalTransform();
				ChildLocal = FTransformConstraintUtils::ComputeRelativeTransform(
					ChildLocal, ChildGlobal, SpaceGlobal, Constraint);
			}
			else
			{
				if (LastConstraint)
				{
					SpaceGlobal = LastConstraint->GetParentGlobalTransform();
					TOptional<FTransform> Relative =
						FTransformConstraintUtils::GetConstraintsRelativeTransform(ConstraintsMinusThis, ChildLocal, ChildGlobal);
					if (Relative)
					{
						ChildLocal = *Relative;
					}
				}
			}
		}
	}
	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StopBaking(MovieScene);
		}
	}
}
void FCompensationEvaluator::ComputeLocalTransformsForBaking(UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer, const TArray<FFrameNumber>& InFrames)
{
	if (InFrames.IsEmpty())
	{
		return;
	}

	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	const TArray< ConstraintPtr > Constraints = GetHandleTransformConstraints(InWorld);
	
	const TArray< ConstraintPtr > ConstraintsMinusThis =
		Constraints.FilterByPredicate([this](const ConstraintPtr& InConstraint)
		{
			return InConstraint != Constraint;
		});

	auto GetLastActiveConstraint = [ConstraintsMinusThis]()
	{
		// find last active constraint in the list that is different than the one we want to compensate for
		const int32 LastActiveIndex = FTransformConstraintUtils::GetLastActiveConstraintIndex(ConstraintsMinusThis);

		// if found, return its parent global transform
		return LastActiveIndex > INDEX_NONE ? Cast<UTickableTransformConstraint>(ConstraintsMinusThis[LastActiveIndex]) : nullptr;
	};

	UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const EMovieScenePlayerStatus::Type PlaybackStatus = InSequencer->GetPlaybackStatus();

	const int32 NumFrames = InFrames.Num();

	ChildLocals.SetNum(NumFrames);
	ChildGlobals.SetNum(NumFrames);
	SpaceGlobals.SetNum(NumFrames);

	// get all constraints for evaluation
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	static constexpr bool bSorted = true;
	const TArray<ConstraintPtr> AllConstraints = Controller.GetAllConstraints(bSorted);

	// avoid transacting when evaluating sequencer
	TGuardValue<ITransaction*> TransactionGuard(GUndo, nullptr);
	
	const TArray<IMovieSceneToolsAnimationBakeHelper*>& BakeHelpers = FMovieSceneToolsModule::Get().GetAnimationBakeHelpers();
	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StartBaking(MovieScene);
		}
	}
	FMovieSceneSequenceTransform RootToLocalTransform = InSequencer->GetFocusedMovieSceneSequenceTransform();

	for (int32 Index = 0; Index < NumFrames; ++Index)
	{
		const FFrameNumber& FrameNumber = (FFrameTime(InFrames[Index]) * RootToLocalTransform.InverseLinearOnly()).GetFrame();

		// evaluate animation
		const FMovieSceneEvaluationRange EvaluationRange = FMovieSceneEvaluationRange(FFrameTime(FrameNumber), TickResolution);
		const FMovieSceneContext Context = FMovieSceneContext(EvaluationRange, PlaybackStatus).SetHasJumped(true);

		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PreEvaluation(MovieScene, FrameNumber);
			}
		}
		InSequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *InSequencer);

		// evaluate constraints
		for (UTickableConstraint* InConstraint : AllConstraints)
		{
			InConstraint->Evaluate(true);
		}

		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PostEvaluation(MovieScene, FrameNumber);
			}
		}

		// evaluate ControlRig?
		// ControlRig->Evaluate_AnyThread();

		FTransform& ChildLocal = ChildLocals[Index];
		FTransform& ChildGlobal = ChildGlobals[Index];
		FTransform& SpaceGlobal = SpaceGlobals[Index];

		// store child transforms        	
		ChildLocal = Handle->GetLocalTransform();
		ChildGlobal = Handle->GetGlobalTransform();

		// store constraint/parent space global transform
		if (const UTickableTransformConstraint* LastConstraint = GetLastActiveConstraint())
		{
			SpaceGlobal = LastConstraint->GetParentGlobalTransform();
			TOptional<FTransform> Relative =
				FTransformConstraintUtils::GetConstraintsRelativeTransform(ConstraintsMinusThis, ChildLocal, ChildGlobal);
			if (Relative)
			{
				ChildLocal = *Relative;
			}
		}
	}
	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StopBaking(MovieScene);
		}
	}

}
void FCompensationEvaluator::ComputeLocalTransformsBeforeDeletion(
	UWorld* InWorld,
	const TSharedPtr<ISequencer>& InSequencer,
	const TArray<FFrameNumber>& InFrames)
{
	if (InFrames.IsEmpty())
	{
		return;
	}

	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	const TArray<ConstraintPtr> Constraints = GetHandleTransformConstraints(InWorld);
	const TArray<ConstraintPtr> ConstraintsMinusThis = Constraints.FilterByPredicate(
		[this](const ConstraintPtr& InConstraint)
		{
			return InConstraint != Constraint;
		});

	// find last active constraint in the list that is different than the on we want to compensate for
	auto GetLastActiveConstraint = [this, ConstraintsMinusThis]()
	{
		// find last active constraint in the list that is different than the on we want to compensate for
		const int32 LastActiveIndex = FTransformConstraintUtils::GetLastActiveConstraintIndex(ConstraintsMinusThis);

		// if found, return its parent global transform
		return LastActiveIndex > INDEX_NONE ? Cast<UTickableTransformConstraint>(ConstraintsMinusThis[LastActiveIndex]) : nullptr;
	};

	// get all constraints for evaluation
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	static constexpr bool bSorted = true;
	const TArray<ConstraintPtr> AllConstraints = Controller.GetAllConstraints(bSorted);
	
	UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const EMovieScenePlayerStatus::Type PlaybackStatus = InSequencer->GetPlaybackStatus();

	const int32 NumFrames = InFrames.Num();

	ChildLocals.SetNum(NumFrames);
	ChildGlobals.SetNum(NumFrames);
	SpaceGlobals.SetNum(NumFrames);
	const TArray<IMovieSceneToolsAnimationBakeHelper*>& BakeHelpers = FMovieSceneToolsModule::Get().GetAnimationBakeHelpers();
	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StartBaking(MovieScene);
		}
	}
	FMovieSceneSequenceTransform RootToLocalTransform = InSequencer->GetFocusedMovieSceneSequenceTransform();

	for (int32 Index = 0; Index < NumFrames; ++Index)
	{
		const FFrameNumber& FrameNumber = (FFrameTime(InFrames[Index]) * RootToLocalTransform.InverseLinearOnly()).GetFrame();

		// evaluate animation
		const FMovieSceneEvaluationRange EvaluationRange = FMovieSceneEvaluationRange(FFrameTime(FrameNumber), TickResolution);
		const FMovieSceneContext Context = FMovieSceneContext(EvaluationRange, PlaybackStatus).SetHasJumped(true);

		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PreEvaluation(MovieScene, FrameNumber);
			}
		}
		InSequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *InSequencer);

		// evaluate constraints
		for (const UTickableConstraint* InConstraint : AllConstraints)
		{
			InConstraint->Evaluate(true);
		}
		
		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PostEvaluation(MovieScene, FrameNumber);
			}
		}
		// evaluate ControlRig?
		// ControlRig->Evaluate_AnyThread();

		FTransform& ChildLocal = ChildLocals[Index];
		FTransform& ChildGlobal = ChildGlobals[Index];
		FTransform& SpaceGlobal = SpaceGlobals[Index];

		// store child transforms        	
		ChildLocal = Handle->GetLocalTransform();
		ChildGlobal = Handle->GetGlobalTransform();

		// store constraint/parent space global transform
		if (const UTickableTransformConstraint* LastConstraint = GetLastActiveConstraint())
		{
			SpaceGlobal = LastConstraint->GetParentGlobalTransform();
			TOptional<FTransform> Relative =
				FTransformConstraintUtils::GetConstraintsRelativeTransform(Constraints, ChildLocal, ChildGlobal);
			if(Relative)
			{
				ChildLocal = *Relative;
			}
		}
	}
	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StopBaking(MovieScene);
		}
	}
}

void FCompensationEvaluator::ComputeCompensation(UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer, const FFrameNumber& InTime)
{
	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	const TArray<ConstraintPtr> Constraints = GetHandleTransformConstraints(InWorld);
	if (Constraints.IsEmpty())
	{
		return;
	}

	// find last active constraint in the list that is different than the on we want to compensate for
	auto GetLastActiveConstraint = [this, Constraints]()
	{
		// find last active constraint in the list that is different than the on we want to compensate for
		const int32 LastActiveIndex = FTransformConstraintUtils::GetLastActiveConstraintIndex(Constraints);

		// if found, return its parent global transform
		return LastActiveIndex > INDEX_NONE ? Cast<UTickableTransformConstraint>(Constraints[LastActiveIndex]) : nullptr;
	};

	// get all constraints for evaluation
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	static constexpr bool bSorted = true;
	const TArray<ConstraintPtr> AllConstraints = Controller.GetAllConstraints(bSorted);
	
	UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	const TArray<IMovieSceneToolsAnimationBakeHelper*>& BakeHelpers = FMovieSceneToolsModule::Get().GetAnimationBakeHelpers();
	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StartBaking(MovieScene);
		}
	}
	
	auto EvaluateAt = [InSequencer, &AllConstraints, &BakeHelpers](FFrameNumber InFrame)
	{
		FMovieSceneSequenceTransform RootToLocalTransform = InSequencer->GetFocusedMovieSceneSequenceTransform();
		InFrame = (FFrameTime(InFrame) * RootToLocalTransform.InverseLinearOnly()).GetFrame();

		UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		const FFrameRate TickResolution = MovieScene->GetTickResolution();
		const EMovieScenePlayerStatus::Type PlaybackStatus = InSequencer->GetPlaybackStatus();

		const FMovieSceneEvaluationRange EvaluationRange0 = FMovieSceneEvaluationRange(FFrameTime(InFrame), TickResolution);
		const FMovieSceneContext Context0 = FMovieSceneContext(EvaluationRange0, PlaybackStatus).SetHasJumped(true);

		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PreEvaluation(MovieScene, InFrame);
			}
		}
		InSequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context0, *InSequencer);

		for (const UTickableConstraint* InConstraint : AllConstraints)
		{
			InConstraint->Evaluate(true);
		}

		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PostEvaluation(MovieScene, InFrame);
			}
		}

		// ControlRig->Evaluate_AnyThread();
	};


	// allocate
	ChildLocals.SetNum(1);
	ChildGlobals.SetNum(1);
	SpaceGlobals.SetNum(1);

	
	// evaluate at InTime and store global
	EvaluateAt(InTime);
	ChildGlobals[0] = Handle->GetGlobalTransform();

	// evaluate at InTime-1 and store local
	EvaluateAt(InTime - 1);
	ChildLocals[0] = Handle->GetLocalTransform();

	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StopBaking(MovieScene);
		}
	}
	
	// if constraint at T-1 then switch to its space
	if (const UTickableTransformConstraint* LastConstraint = GetLastActiveConstraint())
	{
		SpaceGlobals[0] = LastConstraint->GetParentGlobalTransform();
		TOptional<FTransform> Relative =
			FTransformConstraintUtils::GetConstraintsRelativeTransform(Constraints, ChildLocals[0], ChildGlobals[0]);
		if(Relative)
		{
			ChildLocals[0] = *Relative;
		}
	}
	else // switch to parent space
	{
		const FTransform ChildLocal = ChildLocals[0];
		Handle->SetGlobalTransform(ChildGlobals[0]);
		ChildLocals[0] = Handle->GetLocalTransform();
		Handle->SetLocalTransform(ChildLocal);
	}
}

void FCompensationEvaluator::CacheTransforms(UWorld* InWorld, const TSharedPtr<ISequencer>& InSequencer, const TArray<FFrameNumber>& InTime)
{
	if (InTime.IsEmpty())
	{
		return;
	}

	using ConstraintPtr = TObjectPtr<UTickableConstraint>;

	// get all constraints for evaluation
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	static constexpr bool bSorted = true;
	const TArray<ConstraintPtr> AllConstraints = Controller.GetAllConstraints(bSorted);
	
	UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const EMovieScenePlayerStatus::Type PlaybackStatus = InSequencer->GetPlaybackStatus();

	const int32 NumFrames = InTime.Num();

	ChildLocals.SetNum(NumFrames);
	ChildGlobals.SetNum(NumFrames);
	SpaceGlobals.SetNum(NumFrames);

	const TArray<IMovieSceneToolsAnimationBakeHelper*>& BakeHelpers = FMovieSceneToolsModule::Get().GetAnimationBakeHelpers();
	
	auto EvaluateAt = [&](FFrameNumber InFrame)
	{
		const FMovieSceneEvaluationRange EvaluationRange = FMovieSceneEvaluationRange(FFrameTime(InFrame), TickResolution);
		const FMovieSceneContext Context = FMovieSceneContext(EvaluationRange, PlaybackStatus).SetHasJumped(true);
		FMovieSceneSequenceTransform RootToLocalTransform = InSequencer->GetFocusedMovieSceneSequenceTransform();
		InFrame = (FFrameTime(InFrame) * RootToLocalTransform.InverseLinearOnly()).GetFrame();

		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PreEvaluation(MovieScene, InFrame);
			}
		}
		InSequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *InSequencer);

		// evaluate constraints
		for (const UTickableConstraint* InConstraint : AllConstraints)
		{
			InConstraint->Evaluate(true);
		}
		
		for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
		{
			if (BakeHelper)
			{
				BakeHelper->PostEvaluation(MovieScene, InFrame);
			}
		}
	};

	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StartBaking(MovieScene);
		}
	}
	
	for (int32 Index = 0; Index < NumFrames; ++Index)
	{
		// evaluate animation
		EvaluateAt(InTime[Index]);

		// store transforms        	
		ChildLocals[Index] = Handle->GetLocalTransform();
		ChildGlobals[Index] = Handle->GetGlobalTransform();
		SpaceGlobals[Index] = Constraint->GetParentGlobalTransform();
	}
	
	for (IMovieSceneToolsAnimationBakeHelper* BakeHelper : BakeHelpers)
	{
		if (BakeHelper)
		{
			BakeHelper->StopBaking(MovieScene);
		}
	}
}

TArray< TObjectPtr<UTickableConstraint> > FCompensationEvaluator::GetHandleTransformConstraints(UWorld* InWorld) const
{
	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	
	if (Handle)
	{
		// get sorted transform constraints
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
		static constexpr bool bSorted = true;
		const TArray< ConstraintPtr > Constraints = Controller.GetParentConstraints(Handle->GetHash(), bSorted);
		return Constraints.FilterByPredicate([](const ConstraintPtr& InConstraint)
		{
			return IsValid(InConstraint) && InConstraint.IsA<UTickableTransformConstraint>();  
		});
	}

	static const TArray< ConstraintPtr > DummyArray;
	return DummyArray;
}

bool FMovieSceneConstraintChannelHelper::bDoNotCompensate = false;


/*
*
*  Constraint Channel Helpers
*
*/
void FMovieSceneConstraintChannelHelper::HandleConstraintRemoved(
	UTickableConstraint* InConstraint,
	const FMovieSceneConstraintChannel* InConstraintChannel,
	const TSharedPtr<ISequencer>& InSequencer,
	UMovieSceneSection* InSection)
{
	UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(InConstraint);
	if (!Constraint || !Constraint->NeedsCompensation() || !InConstraintChannel || !InSection)
	{
		return;
	}
	
	InSection->Modify();
	TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);
	if (const UTransformableHandle* ControlHandle =Constraint->ChildTRSHandle)
	{
		const TArrayView<const FFrameNumber> Times = InConstraintChannel->GetData().GetTimes();
		if (Times.IsEmpty())
		{
			return;
		}

		// get transform channels
		const TArrayView<FMovieSceneFloatChannel*> FloatTransformChannels = ControlHandle->GetFloatChannels(InSection);
		const TArrayView<FMovieSceneDoubleChannel*> DoubleTransformChannels = ControlHandle->GetDoubleChannels(InSection);

		// get frames after this time
		TArray<FFrameNumber> FramesToCompensate;
		if (FloatTransformChannels.Num() > 0)
		{
			FMovieSceneConstraintChannelHelper::GetFramesWithinActiveState(*InConstraintChannel, FloatTransformChannels, FramesToCompensate);
		}
		else
		{
			FMovieSceneConstraintChannelHelper::GetFramesWithinActiveState(*InConstraintChannel, DoubleTransformChannels, FramesToCompensate);
		}
		// do the compensation
		UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;

		FCompensationEvaluator Evaluator(Constraint);
		Evaluator.ComputeLocalTransformsBeforeDeletion(World, InSequencer, FramesToCompensate);
		const TArray<FTransform>& ChildLocals = Evaluator.ChildLocals;

		const EMovieSceneTransformChannel ChannelsToKey = Constraint->GetChannelsToKey();
		const FFrameRate TickResolution = InSequencer->GetFocusedTickResolution();

		ControlHandle->AddTransformKeys(FramesToCompensate,
			ChildLocals, ChannelsToKey, TickResolution, InSection);
	}
}

void FMovieSceneConstraintChannelHelper::HandleConstraintKeyDeleted(
	UTickableTransformConstraint* InConstraint,
	const FMovieSceneConstraintChannel* InConstraintChannel,
	const TSharedPtr<ISequencer>& InSequencer,
	UMovieSceneSection* InSection,
	const FFrameNumber& InTime)
{
	if (!InConstraint || !InConstraint->NeedsCompensation())
	{
		return;
	}
	
	const FFrameNumber TimeMinusOne(InTime - 1);

	bool CurrentValue = false, PreviousValue = false;
	InConstraintChannel->Evaluate(TimeMinusOne, PreviousValue);
	InConstraintChannel->Evaluate(InTime, CurrentValue);

	if (CurrentValue == PreviousValue)
	{
		const int32 NumKeys = InConstraintChannel->GetNumKeys();
		if (NumKeys > 1)
		{
			return;
		}
	}

	TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);

	if (const UTransformableHandle* ControlHandle = InConstraint->ChildTRSHandle)
	{
		// get transform channels
		const TArrayView<FMovieSceneFloatChannel*> FloatTransformChannels = ControlHandle->GetFloatChannels(InSection);
		const TArrayView<FMovieSceneDoubleChannel*> DoubleTransformChannels = ControlHandle->GetDoubleChannels(InSection);

		// get frames after this time
		TArray<FFrameNumber> FramesToCompensate;
		if (FloatTransformChannels.Num() > 0)
		{
			GetFramesAfter(*InConstraintChannel, InTime, FloatTransformChannels, FramesToCompensate);
		}
		else
		{
			GetFramesAfter(*InConstraintChannel,  InTime, DoubleTransformChannels, FramesToCompensate);
		}
		// do the compensation
		FCompensationEvaluator Evaluator(InConstraint);
		UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
		Evaluator.ComputeLocalTransforms(World, InSequencer, FramesToCompensate, PreviousValue);
		TArray<FTransform>& ChildLocals = Evaluator.ChildLocals;
		//turn off constraint, if we delete the key it may not evaluate to false
		InConstraint->SetActive(false);

		if (ChildLocals.Num() < 2)
		{
			return;
		}
		ChildLocals.RemoveAt(0);

		const EMovieSceneTransformChannel ChannelsToKey = InConstraint->GetChannelsToKey();
		const FFrameRate TickResolution = InSequencer->GetFocusedTickResolution();

		ControlHandle->AddTransformKeys(FramesToCompensate,
			ChildLocals, ChannelsToKey, TickResolution, InSection);

		// now delete any extra TimeMinusOne
		if (FloatTransformChannels.Num() > 0)
		{
			FMovieSceneConstraintChannelHelper::DeleteTransformKeys(FloatTransformChannels, TimeMinusOne);
		}
		else
		{
			FMovieSceneConstraintChannelHelper::DeleteTransformKeys(DoubleTransformChannels, TimeMinusOne);
		}
	}
}

void FMovieSceneConstraintChannelHelper::HandleConstraintKeyMoved(
	const UTickableTransformConstraint* InConstraint,
	const FMovieSceneConstraintChannel* InConstraintChannel,
	UMovieSceneSection* InSection,
	const FFrameNumber& InCurrentFrame, const FFrameNumber& InNextFrame)
{
	const FFrameNumber Delta = InNextFrame - InCurrentFrame;
	if (Delta == 0)
	{
		return;
	}

	if (!InConstraint || !InConstraintChannel || !InSection)
	{
		return;
	}

	if (const UTransformableHandle* ControlHandle = InConstraint->ChildTRSHandle)
	{

		// get transform channels
		const TArrayView<FMovieSceneFloatChannel*> FloatTransformChannels = ControlHandle->GetFloatChannels(InSection);
		const TArrayView<FMovieSceneDoubleChannel*> DoubleTransformChannels = ControlHandle->GetDoubleChannels(InSection);

		// move them
		if (FloatTransformChannels.Num() > 0)
		{
			FMovieSceneConstraintChannelHelper::MoveTransformKeys(FloatTransformChannels, InCurrentFrame, InNextFrame);
		}
		else
		{
			FMovieSceneConstraintChannelHelper::MoveTransformKeys(DoubleTransformChannels, InCurrentFrame, InNextFrame);
		}
	}

}

bool FMovieSceneConstraintChannelHelper::SmartConstraintKey(
	const TSharedPtr<ISequencer>& InSequencer,
	UTickableTransformConstraint* InConstraint, 
	const TOptional<bool>& InOptActive, 
	const TOptional<FFrameNumber>& InOptFrameTime)
{
	if (!InSequencer.IsValid() || !InSequencer->GetFocusedMovieSceneSequence())
	{
		return false;
	}
	
	ITransformConstraintChannelInterface* Interface = GetHandleInterface(InConstraint->ChildTRSHandle);
	if (!Interface)
	{
		return false;
	}
	
	FFrameNumber Time;
	if (InOptFrameTime.IsSet())
	{
		Time = InOptFrameTime.GetValue();
	}
	else
	{
		const FFrameRate TickResolution = InSequencer->GetFocusedTickResolution();
		const FFrameTime FrameTime = InSequencer->GetLocalTime().ConvertTo(TickResolution);
		Time = FrameTime.GetFrame();
	}

	const bool bSucceeded = Interface->SmartConstraintKey(InConstraint, InOptActive, Time, InSequencer);

	//todo need to revisit this to see if we need to create this even if we don't set a key, it's harmless I think either way
	CreateBindingIDForHandle(InSequencer, InConstraint->ChildTRSHandle);
	CreateBindingIDForHandle(InSequencer, InConstraint->ParentTRSHandle);

	return bSucceeded;
}

void FMovieSceneConstraintChannelHelper::Compensate(
	const TSharedPtr<ISequencer>& InSequencer,
	const UTickableTransformConstraint* InConstraint,
	const TOptional<FFrameNumber>& InOptTime)
{
	if (!InSequencer.IsValid() || !InSequencer->GetFocusedMovieSceneSequence())
	{
		return;
	}

	const TObjectPtr<UTransformableHandle>& Handle = InConstraint->ChildTRSHandle;
	
	ITransformConstraintChannelInterface* Interface = GetHandleInterface(Handle);
	if (!Interface)
	{
		return;
	}

	IMovieSceneConstrainedSection* Section = Cast<IMovieSceneConstrainedSection>(Interface->GetHandleConstraintSection(Handle, InSequencer));
	const UWorld* World = Interface->GetHandleWorld(Handle);

	if (!Section || !IsValid(World))
	{
		return;
	}

	CompensateIfNeeded(InSequencer, Section, InOptTime, Handle->GetHash());
}

void FMovieSceneConstraintChannelHelper::CompensateIfNeeded(
	const TSharedPtr<ISequencer>& InSequencer,
	IMovieSceneConstrainedSection* ConstraintSection,
	const TOptional<FFrameNumber>& OptionalTime,
	const int32 InChildHash)
{
	if (bDoNotCompensate)
	{
		return;
	}

	TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);

	// Frames to compensate
	TArray<FFrameNumber> OptionalTimeArray;
	if (OptionalTime.IsSet())
	{
		OptionalTimeArray.Add(OptionalTime.GetValue());
	}

	auto GetConstraintTimesToCompensate = [&OptionalTimeArray](const FConstraintAndActiveChannel& Channel)->TArrayView<const FFrameNumber>
	{
		if (OptionalTimeArray.IsEmpty())
		{
			return Channel.ActiveChannel.GetData().GetTimes();
		}
		return OptionalTimeArray;
	};

	// gather all transform constraints' channels for
	const TArray<FConstraintAndActiveChannel>& ConstraintChannels = ConstraintSection->GetConstraintsChannels();
	TArray<FConstraintAndActiveChannel> TransformConstraintsChannels;
	Algo::CopyIf(ConstraintChannels, TransformConstraintsChannels,
		[InChildHash](const FConstraintAndActiveChannel& InChannel)
		{
			if (!InChannel.Constraint.IsValid())
			{
				return false;
			}

			if ((InChildHash != INDEX_NONE) && (InChannel.Constraint->GetTargetHash() != InChildHash))
			{
				return false;
			}

			const UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(InChannel.Constraint.Get());
			return Constraint && (Constraint->GetTargetHash() == InChildHash) && Constraint->NeedsCompensation();
		}
	);

	// we only need to treat one single constraint per child as FCompensationEvaluator::ComputeCompensation will
	// compensate within the last active constraint's space
	using CompensationData = TPair< UTickableTransformConstraint*, TArray<FFrameNumber> >;
	TArray< CompensationData > ToCompensate;

	// store constraints and times where compensation is needed 
	for (const FConstraintAndActiveChannel& Channel : TransformConstraintsChannels)
	{
		const TArrayView<const FFrameNumber> FramesToCompensate = GetConstraintTimesToCompensate(Channel);
		for (const FFrameNumber& Time : FramesToCompensate)
		{
			const FFrameNumber TimeMinusOne(Time - 1);

			bool CurrentValue = false, PreviousValue = false;
			Channel.ActiveChannel.Evaluate(TimeMinusOne, PreviousValue);
			Channel.ActiveChannel.Evaluate(Time, CurrentValue);

			if (CurrentValue != PreviousValue) //if they are the same no need to do anything
			{
				UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(Channel.Constraint.Get());

				// is the child already in that array?
				int32 DataIndex = ToCompensate.IndexOfByPredicate([Constraint](const CompensationData& InData)
				{
					return InData.Key->GetTargetHash() == Constraint->GetTargetHash();
				});

				// if not, add the constraint
				if (DataIndex == INDEX_NONE)
				{
					DataIndex = ToCompensate.Emplace(Constraint, TArray<FFrameNumber>() );
				}

				// store the time it needs to be compensated at
				TArray<FFrameNumber>& Times = ToCompensate[DataIndex].Value;
				Times.AddUnique(Time);
			}
		}
	}

	// compensate
	bool bNeedsEvaluation = false;
	for (const CompensationData& Data: ToCompensate)
	{
		UTickableTransformConstraint* Constraint = Data.Key;
		const TObjectPtr<UTransformableHandle>& Handle = Constraint->ChildTRSHandle;
		if (ITransformConstraintChannelInterface* Interface = GetHandleInterface(Handle))
		{
			UWorld* World = Interface->GetHandleWorld(Handle);
			
			FCompensationEvaluator Evaluator(Constraint);
			const EMovieSceneTransformChannel ChannelsToKey = Constraint->GetChannelsToKey();
			for (const FFrameNumber& Time : Data.Value)
			{
				// compute transform to set
				// if switching from active to inactive then we must add a key at T-1 in the constraint space
				// if switching from inactive to active then we must add a key at T-1 in the previous constraint or parent space
				Evaluator.ComputeCompensation(World, InSequencer, Time);
				const TArray<FTransform>& LocalTransforms = Evaluator.ChildLocals;

				const FFrameNumber TimeMinusOne(Time - 1);
				Interface->AddHandleTransformKeys(InSequencer, Handle, { TimeMinusOne }, LocalTransforms, ChannelsToKey);

				bNeedsEvaluation = true;
			}
		}
	}

	if (bNeedsEvaluation)
	{
		InSequencer->ForceEvaluate();
	}
}

ITransformConstraintChannelInterface* FMovieSceneConstraintChannelHelper::GetHandleInterface(const UTransformableHandle* InHandle)
{
	if (!IsValid(InHandle) || !InHandle->IsValid())
	{
		return nullptr;
	}

	const FConstraintChannelInterfaceRegistry& InterfaceRegistry = FConstraintChannelInterfaceRegistry::Get();	
	return InterfaceRegistry.FindConstraintChannelInterface(InHandle->GetClass());
}

void FMovieSceneConstraintChannelHelper::CreateBindingIDForHandle(const TSharedPtr<ISequencer>& InSequencer, UTransformableHandle* InHandle)
{
	if (InHandle == nullptr || InSequencer.IsValid() == false)
	{
		return;
	}
	
	if (const USceneComponent* SceneComponent = Cast<USceneComponent>(InHandle->GetTarget().Get()))
	{
		if (AActor* Actor = SceneComponent->GetTypedOuter<AActor>())
		{
			TOptional<FMovieSceneSpawnableAnnotation> Spawnable = FMovieSceneSpawnableAnnotation::Find(Actor);
			if (Spawnable.IsSet())
			{
				// Check whether the spawnable is underneath the current sequence, if so, we can remap it to a local sequence ID
				InHandle->ConstraintBindingID = UE::MovieScene::FRelativeObjectBindingID(InSequencer->GetFocusedTemplateID(), Spawnable->SequenceID, Spawnable->ObjectBindingID,
					*(InSequencer.Get()));
			}
			else
			{
				const FGuid Guid = InSequencer->GetHandleToObject(Actor, false); //don't create it???
				InHandle->ConstraintBindingID = UE::MovieScene::FRelativeObjectBindingID(Guid);
			}
		}
	}
}

void FMovieSceneConstraintChannelHelper::HandleConstraintPropertyChanged(
	UTickableTransformConstraint* InConstraint,
	const FMovieSceneConstraintChannel& InActiveChannel,
	const FPropertyChangedEvent& InPropertyChangedEvent,
	const TSharedPtr<ISequencer>& InSequencer,
	UMovieSceneSection* InSection)
{
	if (!InConstraint || !InSection || !InSequencer.IsValid())
	{
		return;
	}
	
	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();
	if (PropertyName == UTickableParentConstraint::GetScalingPropertyName())
	{
		return CompensateScale(Cast<UTickableParentConstraint>(InConstraint), InActiveChannel, InSequencer, InSection);
	}
}

void FMovieSceneConstraintChannelHelper::CompensateScale(
	UTickableParentConstraint* InParentConstraint,
	const FMovieSceneConstraintChannel& InActiveChannel,
	const TSharedPtr<ISequencer>& InSequencer,
	UMovieSceneSection* InSection)
{
	if (!InParentConstraint)
	{
		return;
	}

	TObjectPtr<UTransformableHandle> Handle = InParentConstraint->ChildTRSHandle;
	ITransformConstraintChannelInterface* Interface = GetHandleInterface(Handle);
	if (!Interface)
	{
		return;
	}

	const TArrayView<const FFrameNumber> Times = InActiveChannel.GetTimes();
	if (Times.IsEmpty())
	{
	    return;
	}

	// get transform channels
	const TArrayView<FMovieSceneFloatChannel*> FloatTransformChannels = Handle->GetFloatChannels(InSection);
	const TArrayView<FMovieSceneDoubleChannel*> DoubleTransformChannels = Handle->GetDoubleChannels(InSection);

	// get frames after this time
	TArray<FFrameNumber> ActiveTimes;
	if (!FloatTransformChannels.IsEmpty())
	{
	    GetFramesWithinActiveState(InActiveChannel, FloatTransformChannels, ActiveTimes);
	}
	else
	{
	    GetFramesWithinActiveState(InActiveChannel, DoubleTransformChannels, ActiveTimes);
	}

	if (ActiveTimes.IsEmpty())
	{
	    return;
	}

	const bool bRefScalingValue = InParentConstraint->IsScalingEnabled();
	
	// if scaling has been enabled (bRefScalingValue == true), it means that it was not before the property has changed so
	// the current scale channels values represent the local scale values of the handle
	// if scaling has been disabled (bRefScalingValue == false), it means that it was before the property has changed so
	// the current scale channels values represent the offset in the constraint space
	
	InParentConstraint->SetScaling(!bRefScalingValue);

	TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);
	InSection->Modify();
	
	FCompensationEvaluator Evaluator(InParentConstraint);
	UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
	Evaluator.CacheTransforms(World, InSequencer, ActiveTimes);

	TArray<FTransform>& ChildLocals = Evaluator.ChildLocals;
	const TArray<FTransform>& ChildGlobals = Evaluator.ChildGlobals;
	const TArray<FTransform>& SpaceGlobals = Evaluator.SpaceGlobals;

	const int32 NumTransforms = ChildLocals.Num();

	if (bRefScalingValue)
	{
	    // local scale values have to be switched to the constraint space to represent the offset
	    for (int Index = 0; Index < NumTransforms; Index++)
	    {
    		FTransform& ChildLocal = ChildLocals[Index];
    		const FTransform Offset = ChildGlobals[Index].GetRelativeTransform(SpaceGlobals[Index]);
    		ChildLocal.SetScale3D(Offset.GetScale3D());
	    }
	}
	// else ChildLocals already represents the data that needs to be keyed as it is the result of
	// the constraint evaluation so it just needs to be keyed

	// add keys
	Interface->AddHandleTransformKeys(InSequencer, Handle, ActiveTimes, ChildLocals, EMovieSceneTransformChannel::Scale);

	// reset scaling to reference value
	InParentConstraint->SetScaling(bRefScalingValue);
}