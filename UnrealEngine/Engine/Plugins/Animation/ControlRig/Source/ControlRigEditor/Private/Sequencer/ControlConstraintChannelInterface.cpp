// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlConstraintChannelInterface.h"

#include "ControlRigSpaceChannelEditors.h"
#include "ISequencer.h"

#include "Constraints/ControlRigTransformableHandle.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "LevelSequence.h"
#include "MovieSceneToolHelpers.h"

#include "TransformConstraint.h"
#include "Algo/Copy.h"
#include "ConstraintsManager.h"
#include "ScopedTransaction.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Constraints/MovieSceneConstraintChannelHelper.inl"

#define LOCTEXT_NAMESPACE "Constraints"

namespace
{
	UMovieScene* GetMovieScene(const TSharedPtr<ISequencer>& InSequencer)
	{
		const UMovieSceneSequence* MovieSceneSequence = InSequencer ? InSequencer->GetFocusedMovieSceneSequence() : nullptr;
		return MovieSceneSequence ? MovieSceneSequence->GetMovieScene() : nullptr;
	}
}

UMovieSceneSection* FControlConstraintChannelInterface::GetHandleSection(
	const UTransformableHandle* InHandle,
	const TSharedPtr<ISequencer>& InSequencer)
{
	if (!IsValid(InHandle) || !InHandle->IsValid())
	{
		return nullptr;
	}
	
	const UTransformableControlHandle* ControlHandle = static_cast<const UTransformableControlHandle*>(InHandle);
	static constexpr bool bConstraintSection = false;
	return GetControlSection(ControlHandle, InSequencer, bConstraintSection);
}

UMovieSceneSection* FControlConstraintChannelInterface::GetHandleConstraintSection(const UTransformableHandle* InHandle, const TSharedPtr<ISequencer>& InSequencer)
{
	if (!IsValid(InHandle) || !InHandle->IsValid())
	{
		return nullptr;
	}

	const UTransformableControlHandle* ControlHandle = static_cast<const UTransformableControlHandle*>(InHandle);
	static constexpr bool bConstraintSection = true;
	return GetControlSection(ControlHandle, InSequencer, bConstraintSection);
}

UWorld* FControlConstraintChannelInterface::GetHandleWorld(UTransformableHandle* InHandle)
{
	if (!IsValid(InHandle) || !InHandle->IsValid())
	{
		return nullptr;
	}
	
	const UTransformableControlHandle* ControlHandle = static_cast<UTransformableControlHandle*>(InHandle);
	const UControlRig* ControlRig = ControlHandle->ControlRig.LoadSynchronous();

	return ControlRig ? ControlRig->GetWorld() : nullptr;
}

bool FControlConstraintChannelInterface::SmartConstraintKey(
	UTickableTransformConstraint* InConstraint, const TOptional<bool>& InOptActive,
	const FFrameNumber& InTime, const TSharedPtr<ISequencer>& InSequencer)
{
	const UTransformableControlHandle* ControlHandle = Cast<UTransformableControlHandle>(InConstraint->ChildTRSHandle);
	if (!ControlHandle)
	{
		return false;
	}

	UMovieSceneControlRigParameterSection* ConstraintSection = GetControlSection(ControlHandle, InSequencer, true);
	UMovieSceneControlRigParameterSection* TransformSection = GetControlSection(ControlHandle, InSequencer, false);
	if ((!ConstraintSection) || (!TransformSection))
	{
		return false;
	}
	
	FScopedTransaction Transaction(LOCTEXT("KeyConstraintaKehy", "Key Constraint Key"));
	ConstraintSection->Modify();
	TransformSection->Modify();

	// set constraint as dynamic
	InConstraint->bDynamicOffset = true;
	
	//check if static if so we need to delete it from world, will get added later again
	if (UConstraintsManager* Manager = InConstraint->GetTypedOuter<UConstraintsManager>())
	{
		Manager->RemoveStaticConstraint(InConstraint);
	}

	// add the channel
	ConstraintSection->AddConstraintChannel(InConstraint);

	// add key if needed
	if (FConstraintAndActiveChannel* Channel = ConstraintSection->GetConstraintChannel(InConstraint->ConstraintID))
	{
		bool ActiveValueToBeSet = false;
		//add key if we can and make sure the key we are setting is what we want
		if (CanAddKey(Channel->ActiveChannel, InTime, ActiveValueToBeSet) && (InOptActive.IsSet() == false || InOptActive.GetValue() == ActiveValueToBeSet))
		{
			const FFrameRate TickResolution = InSequencer->GetFocusedTickResolution();

			const bool bNeedsCompensation = InConstraint->NeedsCompensation();
				
			TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);
			TGuardValue<bool> RemoveConstraintGuard(FConstraintsManagerController::bDoNotRemoveConstraint, true);

			UControlRig* ControlRig = ControlHandle->ControlRig.Get();
			const FName& ControlName = ControlHandle->ControlName;
				
			// store the frames to compensate
			const TArrayView<FMovieSceneFloatChannel*> Channels = ControlHandle->GetFloatChannels(TransformSection);
			TArray<FFrameNumber> FramesToCompensate;
			if (bNeedsCompensation)
			{
				FMovieSceneConstraintChannelHelper::GetFramesToCompensate<FMovieSceneFloatChannel>(Channel->ActiveChannel, ActiveValueToBeSet, InTime, Channels, FramesToCompensate);
			}
			else
			{
				FramesToCompensate.Add(InTime);
			}


			// store child and space transforms for these frames
			FCompensationEvaluator Evaluator(InConstraint);
			Evaluator.ComputeLocalTransforms(ControlRig->GetWorld(), InSequencer, FramesToCompensate, ActiveValueToBeSet);
			TArray<FTransform>& ChildLocals = Evaluator.ChildLocals;
			
			// store tangents at this time
			TArray<FMovieSceneTangentData> Tangents;
			int32 ChannelIndex = 0, NumChannels = 0;

			FChannelMapInfo* pChannelIndex = nullptr;
			FRigControlElement* ControlElement = nullptr;
			Tie(ControlElement, pChannelIndex) = FControlRigSpaceChannelHelpers::GetControlAndChannelInfo(ControlRig, TransformSection, ControlName);

			if (pChannelIndex && ControlElement)
			{
				// get the number of float channels to treat
				NumChannels = FControlRigSpaceChannelHelpers::GetNumFloatChannels(ControlElement->Settings.ControlType);
				if (bNeedsCompensation && NumChannels > 0)
				{
					ChannelIndex = pChannelIndex->ChannelIndex;
					EvaluateTangentAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels, TransformSection, InTime, Tangents);
				}
			}
		
			const EMovieSceneTransformChannel ChannelsToKey =InConstraint->GetChannelsToKey();
			
			// add child's transform key at Time-1 to keep animation
			if (bNeedsCompensation)
			{
				const FFrameNumber TimeMinusOne(InTime - 1);

				ControlHandle->AddTransformKeys({ TimeMinusOne },
					{ ChildLocals[0] }, ChannelsToKey, TickResolution, nullptr,true);

				// set tangents at Time-1
				if (NumChannels > 0) //-V547
				{
					SetTangentsAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels, TransformSection, TimeMinusOne, Tangents);
				}
			}

			// add active key
			{
				TMovieSceneChannelData<bool> ChannelData = Channel->ActiveChannel.GetData();
				ChannelData.AddKey(InTime, ActiveValueToBeSet);
			}

			// compensate
			{
				// we need to remove the first transforms as we store NumFrames+1 transforms
				ChildLocals.RemoveAt(0);

				// add keys
				ControlHandle->AddTransformKeys(FramesToCompensate,
					ChildLocals, ChannelsToKey, TickResolution, nullptr,true);

				// set tangents at Time
				if (bNeedsCompensation && NumChannels > 0)
				{
					SetTangentsAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels, TransformSection, InTime, Tangents);
				}
			}
			return true;
		}
	}
	return false;
}

void FControlConstraintChannelInterface::AddHandleTransformKeys(
	const TSharedPtr<ISequencer>& InSequencer,
	const UTransformableHandle* InHandle,
	const TArray<FFrameNumber>& InFrames,
	const TArray<FTransform>& InLocalTransforms,
	const EMovieSceneTransformChannel& InChannels)
{
	ensure(InLocalTransforms.Num());

	if (!IsValid(InHandle) || !InHandle->IsValid())
	{
		return;
	}

	const UTransformableControlHandle* Handle = static_cast<const UTransformableControlHandle*>(InHandle);
	const UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	UMovieSceneSection* Section = nullptr; //control rig doesn't need section it instead
	Handle->AddTransformKeys(InFrames, InLocalTransforms, InChannels, MovieScene->GetTickResolution(), Section, true);
}

UMovieSceneControlRigParameterSection* FControlConstraintChannelInterface::GetControlSection(
	const UTransformableControlHandle* InHandle,
	const TSharedPtr<ISequencer>& InSequencer,
	const bool bIsConstraint)
{
	if (!IsValid(InHandle) || !InHandle->IsValid())
	{
		return nullptr;
	}
	
	const UControlRig* ControlRig = InHandle->ControlRig.LoadSynchronous();
	if (!ControlRig)
	{
		return nullptr;
	}
	
	const UMovieScene* MovieScene = GetMovieScene(InSequencer);
	if (!MovieScene)
	{
		return nullptr;
	}

	auto GetControlRigTrack = [InHandle, MovieScene]()->UMovieSceneControlRigParameterTrack*
	{
		const TWeakObjectPtr<UControlRig> ControlRig = InHandle->ControlRig.LoadSynchronous();
		if (ControlRig.IsValid())
		{	
			const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
			for (const FMovieSceneBinding& Binding : Bindings)
			{
				UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid());
				UMovieSceneControlRigParameterTrack* ControlRigTrack = Cast<UMovieSceneControlRigParameterTrack>(Track);
				if (ControlRigTrack && ControlRigTrack->GetControlRig() == ControlRig)
				{
					return ControlRigTrack;
				}
			}
		}
		return nullptr;
	};

	UMovieSceneControlRigParameterTrack* ControlRigTrack = GetControlRigTrack();
	if (!ControlRigTrack)
	{
		return nullptr;
	}

	UMovieSceneSection* Section = ControlRigTrack->FindSection(0);
	if (bIsConstraint)
	{
		const TArray<UMovieSceneSection*>& AllSections = ControlRigTrack->GetAllSections();
		if (!AllSections.IsEmpty())
		{
			Section = AllSections[0]; 
		}
	}

	return Cast<UMovieSceneControlRigParameterSection>(Section);
}

#undef LOCTEXT_NAMESPACE
