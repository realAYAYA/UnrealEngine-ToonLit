// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneConstraintChannelHelper.h"
#include "ConstraintChannelHelper.inl"

#include "ISequencer.h"

#include "TransformableHandle.h"
#include "LevelSequence.h"
#include "MovieSceneToolHelpers.h"

#include "TransformConstraint.h"
#include "Algo/Copy.h"

//#include "Tools/BakingHelper.h"
//#include "Tools/ConstraintBaker.h"
#include "ScopedTransaction.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "LevelEditorViewport.h"
#include "Sections/MovieSceneConstrainedSection.h"
#include "MovieSceneToolsModule.h"


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
	
	for (int32 Index = 0; Index < NumFrames + 1; ++Index)
	{
		const FFrameNumber FrameNumber = (Index == 0) ? InFrames[0] - 1 : InFrames[Index - 1];

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
	for (int32 Index = 0; Index < NumFrames; ++Index)
	{
		const FFrameNumber& FrameNumber = InFrames[Index];

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
	for (int32 Index = 0; Index < NumFrames; ++Index)
	{
		const FFrameNumber& FrameNumber = InFrames[Index];

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
	
	auto EvaluateAt = [InSequencer, &AllConstraints, &BakeHelpers](const FFrameNumber InFrame)
	{

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

void FMovieSceneConstraintChannelHelper::CompensateIfNeeded(
	const TSharedPtr<ISequencer>& InSequencer,
	UMovieSceneSection* InSection,
	const TOptional<FFrameNumber>& OptionalTime)
{
	if (bDoNotCompensate)
	{
		return;
	}
	IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(InSection);

	TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);

	// Frames to compensate
	TArray<FFrameNumber> OptionalTimeArray;
	if (OptionalTime.IsSet())
	{
		OptionalTimeArray.Add(OptionalTime.GetValue());
	}

	auto GetSpaceTimesToCompensate = [&OptionalTimeArray](const FConstraintAndActiveChannel& Channel)->TArrayView<const FFrameNumber>
	{
		if (OptionalTimeArray.IsEmpty())
		{
			return Channel.ActiveChannel.GetData().GetTimes();
		}
		return OptionalTimeArray;
	};

	const FFrameRate TickResolution = InSequencer->GetFocusedTickResolution();

	bool bNeedsEvaluation = false;

	// gather all transform constraints
	TArray<FConstraintAndActiveChannel> TransformConstraintsChannels;
	Algo::CopyIf(ConstrainedSection->GetConstraintsChannels(), TransformConstraintsChannels,
		[](const FConstraintAndActiveChannel& InChannel)
		{
			return InChannel.Constraint.IsValid() && InChannel.Constraint->IsA<UTickableTransformConstraint>();
		}
	);

	// compensate constraints
	for (const FConstraintAndActiveChannel& Channel : TransformConstraintsChannels)
	{
		const TArrayView<const FFrameNumber> FramesToCompensate = GetSpaceTimesToCompensate(Channel);
		for (const FFrameNumber& Time : FramesToCompensate)
		{
			const FFrameNumber TimeMinusOne(Time - 1);

			bool CurrentValue = false, PreviousValue = false;
			Channel.ActiveChannel.Evaluate(TimeMinusOne, PreviousValue);
			Channel.ActiveChannel.Evaluate(Time, CurrentValue);

			if (CurrentValue != PreviousValue) //if they are the same no need to do anything
			{
				UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(Channel.Constraint.Get());

				UWorld* World = GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;

				// compute transform to set
				// if switching from active to inactive then we must add a key at T-1 in the constraint space
				// if switching from inactive to active then we must add a key at T-1 in the previous constraint or parent space
				FCompensationEvaluator Evaluator(Constraint);
				Evaluator.ComputeCompensation(World, InSequencer, Time);
				const TArray<FTransform>& LocalTransforms = Evaluator.ChildLocals;

				const EMovieSceneTransformChannel ChannelsToKey = Constraint->GetChannelsToKey();
				if (const UTransformableHandle* ControlHandle = Constraint->ChildTRSHandle)
				{
					ControlHandle->AddTransformKeys({ TimeMinusOne }, LocalTransforms, ChannelsToKey, TickResolution, InSection);
					bNeedsEvaluation = true;
				}
			}
		}
	}

	if (bNeedsEvaluation)
	{
		InSequencer->ForceEvaluate();
	}
}


