// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequencerEditorLibrary.h"
#include "LevelSequence.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealEdGlobals.h"
#include "ISequencer.h"
#include "ControlRigEditorModule.h"
#include "Channels/FloatChannelCurveModel.h"
#include "TransformNoScale.h"
#include "ControlRigComponent.h"
#include "MovieSceneToolHelpers.h"
#include "Rigs/FKControlRig.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "ControlRig.h"
#include "EditMode/ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "ControlRigObjectBinding.h"
#include "Engine/SCS_Node.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ISequencer.h"
#include "Tools/ControlRigTweener.h"
#include "LevelSequencePlayer.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "Logging/LogMacros.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Exporters/AnimSeqExportOption.h"
#include "MovieSceneTimeHelpers.h"
#include "ScopedTransaction.h"
#include "Sequencer/ControlRigParameterTrackEditor.h"
#include "ControlRigSpaceChannelEditors.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Tools/ConstraintBaker.h"
#include "TransformConstraint.h"
#include "Rigs/FKControlRig.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "EditorScriptingHelpers.h"
#include "ConstraintsScripting.h"
#include "Constraints/ControlRigTransformableHandle.h"
#include "Constraints/MovieSceneConstraintChannelHelper.h"
#include "Sections/MovieSceneConstrainedSection.h"
#include "BakingAnimationKeySettings.h"
#include "EditMode/ControlRigEditMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigSequencerEditorLibrary)

#define LOCTEXT_NAMESPACE "ControlrigSequencerEditorLibrary"

TArray<UControlRig*> UControlRigSequencerEditorLibrary::GetVisibleControlRigs()
{
	TArray<UControlRig*> ControlRigs;
	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	if (ControlRigEditMode)
	{
		ControlRigs = ControlRigEditMode->GetControlRigsArray(true /*bIsVisible*/);
	}
	return ControlRigs;
}

TArray<FControlRigSequencerBindingProxy> UControlRigSequencerEditorLibrary::GetControlRigs(ULevelSequence* LevelSequence)
{
	TArray<FControlRigSequencerBindingProxy> ControlRigBindingProxies;
	if (LevelSequence)
	{
		UMovieScene* MovieScene = LevelSequence->GetMovieScene();
		if (MovieScene)
		{
			const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
			for (const FMovieSceneBinding& Binding : Bindings)
			{
				TArray<UMovieSceneTrack*> Tracks = MovieScene->FindTracks(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None);
				for (UMovieSceneTrack* AnyOleTrack : Tracks)
				{
					UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(AnyOleTrack);
					if (Track && Track->GetControlRig())
					{
						FControlRigSequencerBindingProxy BindingProxy;
						BindingProxy.Track = Track;
						
						BindingProxy.ControlRig = Cast<UControlRig>(Track->GetControlRig());
						BindingProxy.Proxy.BindingID = Binding.GetObjectGuid();
						BindingProxy.Proxy.Sequence = LevelSequence;
						ControlRigBindingProxies.Add(BindingProxy);
					}
				}
			}
		}
	}
	return ControlRigBindingProxies;
}

static TArray<UObject*> GetBoundObjects(UWorld* World, ULevelSequence* LevelSequence, const FMovieSceneBindingProxy& Binding, ULevelSequencePlayer** OutPlayer, ALevelSequenceActor** OutActor)
{
	FMovieSceneSequencePlaybackSettings Settings;
	FLevelSequenceCameraSettings CameraSettings;

	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, LevelSequence, Settings, *OutActor);
	*OutPlayer = Player;

	// Evaluation needs to occur in order to obtain spawnables
	Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(LevelSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue().Value, EUpdatePositionMethod::Play));

	FMovieSceneSequenceID SequenceId = Player->State.FindSequenceId(LevelSequence);

	FMovieSceneObjectBindingID ObjectBinding = UE::MovieScene::FFixedObjectBindingID(Binding.BindingID, SequenceId);
	TArray<UObject*> BoundObjects = Player->GetBoundObjects(ObjectBinding);
	
	return BoundObjects;
}

static void AcquireSkeletonAndSkelMeshCompFromObject(UObject* BoundObject, USkeleton** OutSkeleton, USkeletalMeshComponent** OutSkeletalMeshComponent)
{
	*OutSkeletalMeshComponent = nullptr;
	*OutSkeleton = nullptr;
	if (AActor* Actor = Cast<AActor>(BoundObject))
	{
		for (UActorComponent* Component : Actor->GetComponents())
		{
			USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(Component);
			if (SkeletalMeshComp)
			{
				*OutSkeletalMeshComponent = SkeletalMeshComp;
				if (SkeletalMeshComp->GetSkeletalMeshAsset() && SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton())
				{
					*OutSkeleton = SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton();
				}
				return;
			}
		}

		AActor* ActorCDO = Cast<AActor>(Actor->GetClass()->GetDefaultObject());
		if (ActorCDO)
		{
			for (UActorComponent* Component : ActorCDO->GetComponents())
			{
				USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(Component);
				if (SkeletalMeshComp)
				{
					*OutSkeletalMeshComponent = SkeletalMeshComp;
					if (SkeletalMeshComp->GetSkeletalMeshAsset() && SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton())
					{
						*OutSkeleton = SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton();
					}
					return;
				}
			}
		}

		UBlueprintGeneratedClass* ActorBlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(Actor->GetClass());
		if (ActorBlueprintGeneratedClass)
		{
			const TArray<USCS_Node*>& ActorBlueprintNodes = ActorBlueprintGeneratedClass->SimpleConstructionScript->GetAllNodes();

			for (USCS_Node* Node : ActorBlueprintNodes)
			{
				if (Node->ComponentClass && Node->ComponentClass->IsChildOf(USkeletalMeshComponent::StaticClass()))
				{
					USkeletalMeshComponent* SkeletalMeshComp = Cast<USkeletalMeshComponent>(Node->GetActualComponentTemplate(ActorBlueprintGeneratedClass));
					if (SkeletalMeshComp)
					{
						*OutSkeletalMeshComponent = SkeletalMeshComp;
						if (SkeletalMeshComp->GetSkeletalMeshAsset() && SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton())
						{
							*OutSkeleton = SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton();
						}
					}
				}
			}
		}
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(BoundObject))
	{
		*OutSkeletalMeshComponent = SkeletalMeshComponent;
		if (SkeletalMeshComponent->GetSkeletalMeshAsset() && SkeletalMeshComponent->GetSkeletalMeshAsset()->GetSkeleton())
		{
			*OutSkeleton = SkeletalMeshComponent->GetSkeletalMeshAsset()->GetSkeleton();
		}
	}
}

static TSharedPtr<ISequencer> GetSequencerFromAsset()
{
	//if getting sequencer from level sequence need to use the current(leader), not the focused
	ULevelSequence* LevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();
	IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(LevelSequence, false);
	ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);
	TSharedPtr<ISequencer> Sequencer = LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;
	if (Sequencer.IsValid() == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("Can not open Sequencer for the LevelSequence %s"), *(LevelSequence->GetPathName()));
	}
	return Sequencer;
}

static UMovieSceneControlRigParameterTrack* AddControlRig(ULevelSequence* LevelSequence,const UClass* InClass, FGuid ObjectBinding, UControlRig* InExistingControlRig, bool bIsAdditiveControlRig)
{
	FSlateApplication::Get().DismissAllMenus();
	if (!InClass || !InClass->IsChildOf(UControlRig::StaticClass()) ||
		!LevelSequence || !LevelSequence->GetMovieScene())
	{
		return nullptr;
	}
	
	UMovieScene* OwnerMovieScene = LevelSequence->GetMovieScene();
	TSharedPtr<ISequencer> SharedSequencer = GetSequencerFromAsset();
	ISequencer* Sequencer = nullptr; // will be valid  if we have a ISequencer AND it's focused.
	if (SharedSequencer.IsValid() && SharedSequencer->GetFocusedMovieSceneSequence() == LevelSequence)
	{
		Sequencer = SharedSequencer.Get();
	}
	LevelSequence->Modify();
	OwnerMovieScene->Modify();
	
	if (bIsAdditiveControlRig && InClass != UFKControlRig::StaticClass() && !InClass->GetDefaultObject<UControlRig>()->SupportsEvent(FRigUnit_InverseExecution::EventName))
	{
		UE_LOG(LogControlRigEditor, Error, TEXT("Cannot add an additive control rig which does not contain a backwards solve event."));
		return nullptr;
	}
	
	FScopedTransaction AddControlRigTrackTransaction(LOCTEXT("AddControlRigTrack", "Add Control Rig Track"));

	UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(OwnerMovieScene->AddTrack(UMovieSceneControlRigParameterTrack::StaticClass(), ObjectBinding));
	if (Track)
	{
		FString ObjectName = InClass->GetName(); //GetDisplayNameText().ToString();
		ObjectName.RemoveFromEnd(TEXT("_C"));

		bool bSequencerOwnsControlRig = false;
		UControlRig* ControlRig = InExistingControlRig;
		if (ControlRig == nullptr)
		{
			ControlRig = NewObject<UControlRig>(Track, InClass, FName(*ObjectName), RF_Transactional);
			bSequencerOwnsControlRig = true;
		}

		ControlRig->Modify();
		if (UFKControlRig* FKControlRig = Cast<UFKControlRig>(Cast<UControlRig>(ControlRig)))
		{
			if (bIsAdditiveControlRig)
			{
				FKControlRig->SetApplyMode(EControlRigFKRigExecuteMode::Additive);
			}
		}
		else
		{
			ControlRig->SetIsAdditive(bIsAdditiveControlRig);
		}
		ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
		// Do not re-initialize existing control rig
		if (!InExistingControlRig)
		{
			ControlRig->Initialize();
		}
		ControlRig->Evaluate_AnyThread();

		if (SharedSequencer.IsValid())
		{
			SharedSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
		}

		Track->Modify();
		UMovieSceneSection* NewSection = Track->CreateControlRigSection(0, ControlRig, bSequencerOwnsControlRig);
		NewSection->Modify();

		if (bIsAdditiveControlRig)
		{
			const FString AdditiveObjectName = ObjectName + TEXT(" (Layered)");
			Track->SetTrackName(FName(*ObjectName));
			Track->SetDisplayName(FText::FromString(AdditiveObjectName));
			Track->SetColorTint(UMovieSceneControlRigParameterTrack::LayeredRigTrackColor);
		}
		else
		{
			//mz todo need to have multiple rigs with same class
			Track->SetTrackName(FName(*ObjectName));
			Track->SetDisplayName(FText::FromString(ObjectName));
			Track->SetColorTint(UMovieSceneControlRigParameterTrack::AbsoluteRigTrackColor);
		}

		if (SharedSequencer.IsValid())
		{
			SharedSequencer->EmptySelection();
			SharedSequencer->SelectSection(NewSection);
			SharedSequencer->ThrobSectionSelection();
			SharedSequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
			SharedSequencer->ObjectImplicitlyAdded(ControlRig);
		}

		FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
		if (!ControlRigEditMode)
		{
			GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);
			ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));

		}
		if (ControlRigEditMode)
		{
			ControlRigEditMode->AddControlRigObject(ControlRig, SharedSequencer);
		}
		return Track;
	}
	return nullptr;
}

UMovieSceneTrack* UControlRigSequencerEditorLibrary::FindOrCreateControlRigTrack(UWorld* World, ULevelSequence* LevelSequence, const UClass* ControlRigClass, const FMovieSceneBindingProxy& InBinding, bool bIsLayeredControlRig)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	UMovieSceneTrack* BaseTrack = nullptr;
	if (LevelSequence && MovieScene && InBinding.BindingID.IsValid())
	{
		if (const FMovieSceneBinding* Binding = MovieScene->FindBinding(InBinding.BindingID))
		{
			TArray<UMovieSceneTrack*> Tracks = MovieScene->FindTracks(UMovieSceneControlRigParameterTrack::StaticClass(), Binding->GetObjectGuid(), NAME_None);
			for (UMovieSceneTrack* AnyOleTrack : Tracks)
			{
				UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(AnyOleTrack);
				if (Track && Track->GetControlRig() && Track->GetControlRig()->GetClass() == ControlRigClass)
				{
					return Track;
				}
			}

			UMovieSceneControlRigParameterTrack* Track = AddControlRig(LevelSequence, ControlRigClass,  InBinding.BindingID, nullptr, bIsLayeredControlRig);

			if (Track)
			{
				BaseTrack = Track;
			}
		}
	}
	return BaseTrack;
}


TArray<UMovieSceneTrack*> UControlRigSequencerEditorLibrary::FindOrCreateControlRigComponentTrack(UWorld* World, ULevelSequence* LevelSequence, const FMovieSceneBindingProxy& InBinding)
{
	TArray< UMovieSceneTrack*> Tracks;
	TArray<UObject*, TInlineAllocator<1>> Result;
	if (LevelSequence == nullptr || LevelSequence->GetMovieScene() == nullptr)
	{
		return Tracks;
	}
	UMovieScene* MovieScene = LevelSequence->GetMovieScene();
	UObject* Context = nullptr;
	ALevelSequenceActor* OutActor = nullptr;
	ULevelSequencePlayer* OutPlayer = nullptr;
	Result = GetBoundObjects(World, LevelSequence, InBinding, &OutPlayer, &OutActor);
	if (Result.Num() > 0 && Result[0])
	{
		UObject* BoundObject = Result[0];
		if (AActor* BoundActor = Cast<AActor>(BoundObject))
		{
			TArray<UControlRigComponent*> ControlRigComponents;
			BoundActor->GetComponents<UControlRigComponent>(ControlRigComponents);
			for (UControlRigComponent* ControlRigComponent : ControlRigComponents)
			{
				if (UControlRig* CR = Cast<UControlRig>(ControlRigComponent->GetControlRig()))
				{
					UMovieSceneControlRigParameterTrack* GoodTrack = nullptr;
					if (FMovieSceneBinding* Binding = MovieScene->FindBinding(InBinding.BindingID))
					{
						for (UMovieSceneTrack* Track : Binding->GetTracks())
						{
							if (UMovieSceneControlRigParameterTrack* ControlRigParameterTrack = Cast<UMovieSceneControlRigParameterTrack>(Track))
							{
								if (ControlRigParameterTrack->GetControlRig() == CR)
								{
									GoodTrack = ControlRigParameterTrack;
									break;
								}
							}
						}
					}

					if (GoodTrack == nullptr)
					{
						GoodTrack = AddControlRig(LevelSequence, CR->GetClass(), InBinding.BindingID, CR, false);
					}
					Tracks.Add(GoodTrack);
				}
			}
		}
	}

	if (OutActor)
	{
		World->DestroyActor(OutActor);
	}

	return Tracks;
}

bool UControlRigSequencerEditorLibrary::TweenControlRig(ULevelSequence* LevelSequence, UControlRig* ControlRig, float TweenValue)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() && WeakSequencer.Pin()->GetFocusedMovieSceneSequence() == LevelSequence
		&& ControlRig && LevelSequence->GetMovieScene())
	{
		FControlsToTween ControlsToTween;
		LevelSequence->GetMovieScene()->Modify();
		TArray<UControlRig*> SelectedControlRigs;
		SelectedControlRigs.Add(ControlRig);
		ControlsToTween.Setup(SelectedControlRigs, WeakSequencer);
		ControlsToTween.Blend(WeakSequencer, TweenValue);
		return true;
	}
	return false;
}

bool UControlRigSequencerEditorLibrary::BlendValuesOnSelected(ULevelSequence* LevelSequence, EAnimToolBlendOperation BlendOperation, float BlendValue)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() && WeakSequencer.Pin()->GetFocusedMovieSceneSequence() == LevelSequence
		&& LevelSequence->GetMovieScene())
	{
		if (FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
		{
			TWeakPtr<FControlRigEditMode> WeakMode = StaticCastSharedRef<FControlRigEditMode, FEdMode>(EditMode->AsShared()).ToWeakPtr();
			
			FControlsToTween ControlsToTween;
			LevelSequence->GetMovieScene()->Modify();
			switch(BlendOperation)
			{ 
			case EAnimToolBlendOperation::Tween:
				{
					FControlsToTween BlendTool;
					BlendTool.Setup(WeakSequencer, WeakMode);
					BlendTool.Blend(WeakSequencer, BlendValue);
					return true;
				}
			case EAnimToolBlendOperation::BlendToNeighbor:
				{
					FBlendNeighborSlider BlendTool;
					BlendTool.Setup(WeakSequencer, WeakMode);
					BlendTool.Blend(WeakSequencer, BlendValue);
					return true;
				}
			case EAnimToolBlendOperation::PushPull:
				{
					FPushPullSlider BlendTool;
					BlendTool.Setup(WeakSequencer, WeakMode);
					BlendTool.Blend(WeakSequencer, BlendValue);
					return true;
				}
			case EAnimToolBlendOperation::BlendRelative:
				{
					FBlendRelativeSlider BlendTool;
					BlendTool.Setup(WeakSequencer, WeakMode);
					BlendTool.Blend(WeakSequencer, BlendValue);
					return true;
				}
			case EAnimToolBlendOperation::BlendToEase:
				{
					FBlendToEaseSlider BlendTool;
					BlendTool.Setup(WeakSequencer, WeakMode);
					BlendTool.Blend(WeakSequencer, BlendValue);
					return true;
				}
			case EAnimToolBlendOperation::SmoothRough:
				{
					FSmoothRoughSlider BlendTool;
					BlendTool.Setup(WeakSequencer, WeakMode);
					BlendTool.Blend(WeakSequencer, BlendValue);
					return true;
				}
			}
		}
	}
	return false;
}

UTickableConstraint* UControlRigSequencerEditorLibrary::AddConstraint(UWorld* World, ETransformConstraintType InType, UTransformableHandle* InChildHandle, UTransformableHandle* InParentHandle, const bool bMaintainOffset)
{
	if (!World)
	{
		UE_LOG(LogControlRig, Error, TEXT("AddConstraint: Need Valid World"));
		return nullptr;
	}

	UTickableTransformConstraint* Constraint = FTransformConstraintUtils::CreateFromType(World, InType);
	if (!Constraint)
	{
		UE_LOG(LogControlRig, Error, TEXT("AddConstraint: Can not create constraint from type"));
		return nullptr;
	}

	if(FTransformConstraintUtils::AddConstraint(World, InParentHandle, InChildHandle, Constraint, bMaintainOffset) == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("AddConstraint: Constraint not added"));
		Constraint->MarkAsGarbage();
		return nullptr;
	}

	if (ULevelSequence* LevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence())
	{
		//add key
		const TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
		if (WeakSequencer.IsValid())
		{
			FMovieSceneConstraintChannelHelper::SmartConstraintKey(WeakSequencer.Pin(), Constraint, TOptional<bool>(), TOptional<FFrameNumber>());
		}
	}
	else
	{
		FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		Controller.StaticConstraintCreated(World, Constraint);
	}
	return Constraint;
}

TArray <UTickableConstraint*> UControlRigSequencerEditorLibrary::GetConstraintsForHandle(UWorld* InWorld, const UTransformableHandle* InChild)
{
	TArray <UTickableConstraint*> Constraints;
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	const TArray<TWeakObjectPtr<UTickableConstraint>>& AllConstraints = Controller.GetAllConstraints(false);
	for (const TWeakObjectPtr<UTickableConstraint>& TickConstraint : AllConstraints)
	{
		if (TObjectPtr<UTickableTransformConstraint> Constraint = Cast<UTickableTransformConstraint>(TickConstraint.Get()))
		{
			if (Constraint->ChildTRSHandle == InChild)
			{
				Constraints.Add(Constraint.Get());
			}
		}
	}
	return Constraints;
}

bool UControlRigSequencerEditorLibrary::Compensate(UTickableConstraint* InConstraint, FFrameNumber InTime, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("Compensate: Need open Sequencer"));
		return false;
	}
	if (UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(InConstraint))
	{
		TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			InTime = FFrameRate::TransformTime(FFrameTime(InTime, 0), DisplayRate, TickResolution).RoundToFrame();
		}
		TOptional<FFrameNumber> OptTime(InTime);
		FMovieSceneConstraintChannelHelper::Compensate(Sequencer, Constraint, OptTime);
		return true;
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("Compensate: Not Transform Constraint"));
	}
	return false;
}

bool UControlRigSequencerEditorLibrary::CompensateAll(UTickableConstraint* InConstraint)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("CompensateAll: Need open Sequencer"));
		return false;
	}
	if (UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(InConstraint))
	{
		FMovieSceneConstraintChannelHelper::Compensate(WeakSequencer.Pin(), Constraint, TOptional<FFrameNumber>());
		return true;
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("CompensateAll: Not Transform Constraint"));
	}
	return false;
}

bool UControlRigSequencerEditorLibrary::SetConstraintActiveKey(UTickableConstraint* InConstraint, bool bActive, FFrameNumber InTime, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("SetConstraintActiveKey: Need open Sequencer"));
		return false;
	}

	if (UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(InConstraint))
	{
		TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();

		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			InTime = FFrameRate::TransformTime(FFrameTime(InTime, 0), DisplayRate, TickResolution).RoundToFrame();
		}
		return FMovieSceneConstraintChannelHelper::SmartConstraintKey(Sequencer, Constraint, TOptional<bool>(bActive), TOptional<FFrameNumber>(InTime));
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("SetConstraintActiveKey: Not Transform Constraint"));
	}
	return false;
}

bool UControlRigSequencerEditorLibrary::GetConstraintKeys(UTickableConstraint* InConstraint, UMovieSceneSection* ConstraintSection, TArray<bool>& OutBools, TArray<FFrameNumber>& OutFrames, EMovieSceneTimeUnit TimeUnit)
{
	if (InConstraint == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("GetConstraintKeys: Constraint not valid"));
		return false;
	}
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("GetConstraintKeys: Need open Sequencer"));
		return false;
	}

	IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(ConstraintSection);
	if (ConstrainedSection == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("GetConstraintKeys: Section doesn't support constraints"));
		return false;
	}
	FConstraintAndActiveChannel* ConstraintAndChannel = ConstrainedSection->GetConstraintChannel(InConstraint->ConstraintID);
	if (ConstraintAndChannel == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("GetConstraintKeys: Constraint not found in section"));
		return false;
	}

	if (UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(InConstraint))
	{
		TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();


		TArray<FFrameNumber> OurKeyTimes;
		TArray<FKeyHandle> OurKeyHandles;
		TRange<FFrameNumber> CurrentFrameRange;
		CurrentFrameRange.SetLowerBound(TRangeBound<FFrameNumber>());
		CurrentFrameRange.SetUpperBound(TRangeBound<FFrameNumber>());

		TMovieSceneChannelData<bool> ChannelInterface = ConstraintAndChannel->ActiveChannel.GetData();
		ChannelInterface.GetKeys(CurrentFrameRange, &OurKeyTimes, &OurKeyHandles);
		
		if (OurKeyTimes.Num() > 0)
		{
			OutBools.SetNum(OurKeyTimes.Num());
			OutFrames.SetNum(OurKeyTimes.Num());
			int32 Index = 0;
			for (FFrameNumber Frame : OurKeyTimes)
			{
				ConstraintAndChannel->ActiveChannel.Evaluate(FFrameTime(Frame, 0.0f), OutBools[Index]);
				if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
				{
					Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), TickResolution,DisplayRate).RoundToFrame();
				}
				OutFrames[Index] = Frame;
				++Index;
			}
		}
		else
		{
			OutBools.SetNum(0);
			OutFrames.SetNum(0);
		}
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("GetConstraintKeys: Not Transform Constraint"));
		return false;
	}
	return true;
}


bool UControlRigSequencerEditorLibrary::MoveConstraintKey(UTickableConstraint* Constraint, UMovieSceneSection* ConstraintSection, FFrameNumber InTime, FFrameNumber InNewTime, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("MoveConstraintKey: Need open Sequencer"));
		return false;
	}
	IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(ConstraintSection);
	if (ConstrainedSection == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("MoveConstraintKey: Section doesn't support constraints"));
		return false;
	}
	if (Constraint == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("MoveConstraintKey: Constraint not valid"));
		return false;
	}
	FConstraintAndActiveChannel* ConstraintAndChannel = ConstrainedSection->GetConstraintChannel(Constraint->ConstraintID);
	if (ConstraintAndChannel == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("MoveConstraintKey: Constraint not found in section"));
		return false;
	}
	TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
	FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
	FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();

	if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
	{
		InTime = FFrameRate::TransformTime(FFrameTime(InTime, 0), DisplayRate, TickResolution).RoundToFrame();
		InNewTime = FFrameRate::TransformTime(FFrameTime(InNewTime, 0), DisplayRate, TickResolution).RoundToFrame();
	}

	TArray<FFrameNumber> OurKeyTimes;
	TArray<FKeyHandle> OurKeyHandles;
	TRange<FFrameNumber> CurrentFrameRange;
	CurrentFrameRange.SetLowerBound(TRangeBound<FFrameNumber>(InTime));
	CurrentFrameRange.SetUpperBound(TRangeBound<FFrameNumber>(InTime));
	TMovieSceneChannelData<bool> ChannelInterface = ConstraintAndChannel->ActiveChannel.GetData();
	ChannelInterface.GetKeys(CurrentFrameRange, &OurKeyTimes, &OurKeyHandles);
	if (OurKeyHandles.Num() > 0)
	{
		ConstraintSection->TryModify();
		TArray<FFrameNumber> NewKeyTimes;
		NewKeyTimes.Add(InNewTime);
		ConstraintAndChannel->ActiveChannel.SetKeyTimes(OurKeyHandles, NewKeyTimes);
		WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("MoveConstraintKey: No Key To Move At that Time"));
		return false;
	}
	return true;
}

bool UControlRigSequencerEditorLibrary::DeleteConstraintKey(UTickableConstraint* Constraint, UMovieSceneSection* ConstraintSection, FFrameNumber InTime, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("DeleteConstraintKey: Need open Sequencer"));
		return false;
	}
	IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(ConstraintSection);
	if (ConstrainedSection == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("DeleteConstraintKey: Section doesn't support constraints"));
		return false;
	}
	if (Constraint == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("DeleteConstraintKey: Constraint not valid"));
		return false;
	}
	FConstraintAndActiveChannel* ConstraintAndChannel = ConstrainedSection->GetConstraintChannel(Constraint->ConstraintID);
	if (ConstraintAndChannel == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("DeleteConstraintKey: Constraint not found in section"));
		return false;
	}
	TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
	FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
	FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();

	if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
	{
		InTime = FFrameRate::TransformTime(FFrameTime(InTime, 0), DisplayRate, TickResolution).RoundToFrame();
	}

	TArray<FFrameNumber> OurKeyTimes;
	TArray<FKeyHandle> OurKeyHandles;
	TRange<FFrameNumber> CurrentFrameRange;
	CurrentFrameRange.SetLowerBound(TRangeBound<FFrameNumber>(InTime));
	CurrentFrameRange.SetUpperBound(TRangeBound<FFrameNumber>(InTime));
	TMovieSceneChannelData<bool> ChannelInterface = ConstraintAndChannel->ActiveChannel.GetData();
	ChannelInterface.GetKeys(CurrentFrameRange, &OurKeyTimes, &OurKeyHandles);
	if (OurKeyHandles.Num() > 0)
	{
		ConstraintSection->TryModify();
		ConstraintAndChannel->ActiveChannel.DeleteKeys(OurKeyHandles);
		WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("DeleteConstraintKey: No Key To Delete At that Time"));
		return false;
	}
	return true;
}

bool UControlRigSequencerEditorLibrary::BakeConstraint(UWorld* World, UTickableConstraint* Constraint, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	if (!World)
	{
		UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need Valid World"));
		return false;
	}
	TOptional< FBakingAnimationKeySettings> Settings;

	if (UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint))
	{
		TSharedPtr<ISequencer> Sequencer = GetSequencerFromAsset();
		if (!Sequencer || !Sequencer->GetFocusedMovieSceneSequence())
		{
			UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need loaded level Sequence"));
			return false;
		}
		const UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		if (!MovieScene)
		{
			UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need valid Movie Scene"));
			return false;
		}
		
		TOptional<TArray<FFrameNumber>> FramesToBake;
		TArray<FFrameNumber> RealFramesToBake;
		RealFramesToBake.SetNum(Frames.Num());

		int32 Index = 0;
		for (FFrameNumber Frame : Frames) 
		{
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			RealFramesToBake[Index++] = Frame;
		} 
		if (RealFramesToBake.Num() > 0)
		{
			FramesToBake = RealFramesToBake;
		}
		FConstraintBaker::Bake(World, TransformConstraint, Sequencer, Settings, FramesToBake);
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need Valid Constraint"));
		return false;
	}
	return true;
}

bool UControlRigSequencerEditorLibrary::BakeConstraints(UWorld* World, TArray<UTickableConstraint*>& InConstraints, const FBakingAnimationKeySettings& InSettings)
{
	if (!World)
	{
		UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need Valid World"));
		return false;
	}

	TSharedPtr<ISequencer> Sequencer = GetSequencerFromAsset();
	if (!Sequencer || !Sequencer->GetFocusedMovieSceneSequence())
	{
		UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need loaded level Sequence"));
		return false;
	}
	const UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!MovieScene)
	{
		UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need valid Movie Scene"));
		return false;
	}
	TArray<UTickableTransformConstraint*> TransformConstraints;
	for (UTickableConstraint* Constraint : InConstraints)
	{
		if (UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint))
		{
			TransformConstraints.Add(TransformConstraint);
		}
		else
		{
			UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need Valid Constraint"));
			return false;
		}
	}

	if(FConstraintBaker::BakeMultiple(World, TransformConstraints, Sequencer, InSettings) == false)
	{
		UE_LOG(LogControlRig, Error, TEXT("BakeMultiple: Failed"));
		return false;
	}
	return true;
}


bool UControlRigSequencerEditorLibrary::SnapControlRig(ULevelSequence* LevelSequence, FFrameNumber StartFrame, FFrameNumber EndFrame, const FControlRigSnapperSelection& ChildrenToSnap,
	const FControlRigSnapperSelection& ParentToSnap, const UControlRigSnapSettings* SnapSettings, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || LevelSequence->GetMovieScene() == nullptr)
	{
		return false;
	}
	FControlRigSnapper Snapper;
	if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
	{
		StartFrame = FFrameRate::TransformTime(FFrameTime(StartFrame, 0), LevelSequence->GetMovieScene()->GetDisplayRate(), LevelSequence->GetMovieScene()->GetTickResolution()).RoundToFrame();
		EndFrame = FFrameRate::TransformTime(FFrameTime(EndFrame, 0), LevelSequence->GetMovieScene()->GetDisplayRate(), LevelSequence->GetMovieScene()->GetTickResolution()).RoundToFrame();

	}
	return Snapper.SnapIt(StartFrame, EndFrame, ChildrenToSnap, ParentToSnap,SnapSettings);
}

FTransform UControlRigSequencerEditorLibrary::GetActorWorldTransform(ULevelSequence* LevelSequence, AActor* Actor, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	TArray<FFrameNumber> Frames;
	Frames.Add(Frame);
	TArray<FTransform> Transforms = GetActorWorldTransforms(LevelSequence, Actor, Frames, TimeUnit);
	if (Transforms.Num() == 1)
	{
		return Transforms[0];
	}
	return FTransform::Identity;
}
static void ConvertFramesToTickResolution(UMovieScene* MovieScene, const TArray<FFrameNumber>& InFrames, EMovieSceneTimeUnit TimeUnit, TArray<FFrameNumber>& OutFrames)
{
	OutFrames.SetNum(InFrames.Num());
	int32 Index = 0;
	for (const FFrameNumber& Frame : InFrames)
	{
		FFrameTime FrameTime(Frame);
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			FrameTime = FFrameRate::TransformTime(FrameTime, MovieScene->GetDisplayRate(), MovieScene->GetTickResolution());
		}
		OutFrames[Index++] = FrameTime.RoundToFrame();
	}
}

TArray<FTransform> UControlRigSequencerEditorLibrary::GetActorWorldTransforms(ULevelSequence* LevelSequence,AActor* Actor, const TArray<FFrameNumber>& InFrames, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	TArray<FTransform> OutWorldTransforms;
	if (WeakSequencer.IsValid() && Actor)
	{
		FActorForWorldTransforms Actors;
		Actors.Actor = Actor;
		TArray<FFrameNumber> Frames;
		ConvertFramesToTickResolution(LevelSequence->GetMovieScene(), InFrames, TimeUnit, Frames);
		MovieSceneToolHelpers::GetActorWorldTransforms(WeakSequencer.Pin().Get(), Actors, Frames, OutWorldTransforms); 
	}
	return OutWorldTransforms;

}

FTransform UControlRigSequencerEditorLibrary::GetSkeletalMeshComponentWorldTransform(ULevelSequence* LevelSequence,USkeletalMeshComponent* SkeletalMeshComponent, FFrameNumber Frame, 
	EMovieSceneTimeUnit TimeUnit, FName SocketName)
{
	TArray<FFrameNumber> Frames;
	Frames.Add(Frame);
	TArray<FTransform> Transforms = GetSkeletalMeshComponentWorldTransforms(LevelSequence, SkeletalMeshComponent, Frames, TimeUnit,SocketName);
	if (Transforms.Num() == 1)
	{
		return Transforms[0];
	}
	return FTransform::Identity;
}

TArray<FTransform> UControlRigSequencerEditorLibrary::GetSkeletalMeshComponentWorldTransforms(ULevelSequence* LevelSequence,USkeletalMeshComponent* SkeletalMeshComponent, const TArray<FFrameNumber>& InFrames,
	EMovieSceneTimeUnit TimeUnit, FName SocketName)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	TArray<FTransform> OutWorldTransforms;
	if (WeakSequencer.IsValid() && SkeletalMeshComponent)
	{
		FActorForWorldTransforms Actors;
		AActor* Actor = SkeletalMeshComponent->GetTypedOuter<AActor>();
		if (Actor)
		{
			Actors.Actor = Actor;
			Actors.Component = SkeletalMeshComponent;
			Actors.SocketName = SocketName;
			TArray<FFrameNumber> Frames;
			ConvertFramesToTickResolution(LevelSequence->GetMovieScene(), InFrames, TimeUnit, Frames);
			MovieSceneToolHelpers::GetActorWorldTransforms(WeakSequencer.Pin().Get(), Actors, Frames, OutWorldTransforms);
		}
	}
	return OutWorldTransforms;
}

FTransform UControlRigSequencerEditorLibrary::GetControlRigWorldTransform(ULevelSequence* LevelSequence,UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,
	EMovieSceneTimeUnit TimeUnit)
{
	TArray<FFrameNumber> Frames;
	Frames.Add(Frame);
	TArray<FTransform> Transforms = GetControlRigWorldTransforms(LevelSequence, ControlRig, ControlName, Frames, TimeUnit);
	if (Transforms.Num() == 1)
	{
		return Transforms[0];
	}
	return FTransform::Identity;
}

TArray<FTransform> UControlRigSequencerEditorLibrary::GetControlRigWorldTransforms(ULevelSequence* LevelSequence,UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& InFrames,
	EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	TArray<FTransform> OutWorldTransforms;
	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
		{
			USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
			if (Component)
			{
				AActor* Actor = Component->GetTypedOuter< AActor >();
				if (Actor)
				{
					ControlRig->Modify();
					TArray<FTransform> ControlRigParentWorldTransforms;
					FActorForWorldTransforms ControlRigActorSelection;
					ControlRigActorSelection.Actor = Actor;
					TArray<FFrameNumber> Frames;
					ConvertFramesToTickResolution(LevelSequence->GetMovieScene(), InFrames, TimeUnit, Frames);
					MovieSceneToolHelpers::GetActorWorldTransforms(WeakSequencer.Pin().Get(), ControlRigActorSelection, Frames, ControlRigParentWorldTransforms);
					FControlRigSnapper Snapper;
					Snapper.GetControlRigControlTransforms(WeakSequencer.Pin().Get(), ControlRig, ControlName, Frames, ControlRigParentWorldTransforms, OutWorldTransforms);	
				}
			}
		}
	}
	return OutWorldTransforms;
}


static void LocalSetControlRigWorldTransforms(ULevelSequence* LevelSequence,UControlRig* ControlRig, FName ControlName, EControlRigSetKey SetKey, const TArray<FFrameNumber>& InFrames, 
	const TArray<FTransform>& WorldTransforms, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
		{
			USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
			if (Component)
			{
				AActor* Actor = Component->GetTypedOuter< AActor >();
				if (Actor)
				{
					TArray<FFrameNumber> Frames;
					ConvertFramesToTickResolution(LevelSequence->GetMovieScene(), InFrames, TimeUnit, Frames);

					UMovieScene* MovieScene = WeakSequencer.Pin().Get()->GetFocusedMovieSceneSequence()->GetMovieScene();
					MovieScene->Modify();
					FFrameRate TickResolution = MovieScene->GetTickResolution();
					FRigControlModifiedContext Context;
					Context.SetKey = SetKey;

					ControlRig->Modify();
					TArray<FTransform> ControlRigParentWorldTransforms;
					FActorForWorldTransforms ControlRigActorSelection;
					ControlRigActorSelection.Actor = Actor;
					MovieSceneToolHelpers::GetActorWorldTransforms(WeakSequencer.Pin().Get(), ControlRigActorSelection, Frames, ControlRigParentWorldTransforms);
					FControlRigSnapper Snapper;
					
					TArray<FFrameNumber> OneFrame;
					OneFrame.SetNum(1);
					TArray<FTransform> CurrentControlRigTransform, CurrentParentWorldTransform;
					CurrentControlRigTransform.SetNum(1);
					CurrentParentWorldTransform.SetNum(1);
					
					for (int32 Index = 0; Index < WorldTransforms.Num(); ++Index)
					{
						OneFrame[0] = Frames[Index];
						CurrentParentWorldTransform[0] = ControlRigParentWorldTransforms[Index];
						//this will evaluate at the current frame which we want
						Snapper.GetControlRigControlTransforms(WeakSequencer.Pin().Get(), ControlRig, ControlName, OneFrame, CurrentParentWorldTransform, CurrentControlRigTransform);
							
						const FFrameNumber& FrameNumber = Frames[Index];
						Context.LocalTime = TickResolution.AsSeconds(FFrameTime(FrameNumber));
						FTransform GlobalTransform = WorldTransforms[Index].GetRelativeTransform(ControlRigParentWorldTransforms[Index]);
						ControlRig->SetControlGlobalTransform(ControlName, GlobalTransform, true, Context, false /*undo*/, false /*bPrintPython*/, true/* bFixEulerFlips*/);
					}
				}
			}
		}
	}
}

void UControlRigSequencerEditorLibrary::SetControlRigWorldTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, const FTransform& WorldTransform,
	EMovieSceneTimeUnit TimeUnit, bool bSetKey)
{
	EControlRigSetKey SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
	TArray<FFrameNumber> Frames;
	Frames.Add(Frame);
	TArray<FTransform> WorldTransforms;
	WorldTransforms.Add(WorldTransform);

	LocalSetControlRigWorldTransforms(LevelSequence, ControlRig, ControlName, SetKey, Frames, WorldTransforms, TimeUnit);

}

void UControlRigSequencerEditorLibrary::SetControlRigWorldTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FTransform>& WorldTransforms,
	EMovieSceneTimeUnit TimeUnit)
{
	LocalSetControlRigWorldTransforms(LevelSequence, ControlRig, ControlName, EControlRigSetKey::Always, Frames, WorldTransforms,TimeUnit);
}

bool UControlRigSequencerEditorLibrary::BakeToControlRig(UWorld* World, ULevelSequence* LevelSequence, UClass* InClass, UAnimSeqExportOption* ExportOptions, bool bReduceKeys, float Tolerance,
	const FMovieSceneBindingProxy& Binding, bool bResetControls)
{
	UMovieScene* MovieScene = LevelSequence->GetMovieScene();
	if (Binding.Sequence != LevelSequence)
	{
		UE_LOG(LogControlRig, Error, TEXT("Baking: Binding.Sequence different"));
		return false;
	}
	//get level sequence if one exists...
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	ALevelSequenceActor* OutActor = nullptr;
	FMovieSceneSequencePlaybackSettings Settings;
	FLevelSequenceCameraSettings CameraSettings;
	FMovieSceneSequenceIDRef Template = MovieSceneSequenceID::Root;
	FMovieSceneSequenceTransform RootToLocalTransform;
	IMovieScenePlayer* Player = nullptr;
	ULevelSequencePlayer* LevelPlayer = nullptr;
	if (WeakSequencer.IsValid())
	{
		Player = WeakSequencer.Pin().Get();
	}
	else
	{
		Player = LevelPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(World, LevelSequence, Settings, OutActor);
	}
	if (Player == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("Baking: Problem Setting up Player"));
		return false;
	}
	
	

	bool bResult = false;
	const FScopedTransaction Transaction(LOCTEXT("BakeToControlRig_Transaction", "Bake To Control Rig"));
	{
		FSpawnableRestoreState SpawnableRestoreState(MovieScene);

		if (LevelPlayer && SpawnableRestoreState.bWasChanged)
		{
			// Evaluate at the beginning of the subscene time to ensure that spawnables are created before export
			FFrameTime StartTime = FFrameRate::TransformTime(UE::MovieScene::DiscreteInclusiveLower(MovieScene->GetPlaybackRange()).Value, MovieScene->GetTickResolution(), MovieScene->GetDisplayRate());
			LevelPlayer->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(StartTime, EUpdatePositionMethod::Play));
		}
		TArrayView<TWeakObjectPtr<>>  Result = Player->FindBoundObjects(Binding.BindingID, Template);
	
		if (Result.Num() > 0 && Result[0].IsValid())
		{
			UObject* BoundObject = Result[0].Get();
			USkeleton* Skeleton = nullptr;
			USkeletalMeshComponent* SkeletalMeshComp = nullptr;
			AcquireSkeletonAndSkelMeshCompFromObject(BoundObject, &Skeleton, &SkeletalMeshComp);
			if (SkeletalMeshComp && SkeletalMeshComp->GetSkeletalMeshAsset() && SkeletalMeshComp->GetSkeletalMeshAsset()->GetSkeleton())
			{
				UAnimSequence* TempAnimSequence = NewObject<UAnimSequence>(GetTransientPackage(), NAME_None);
				TempAnimSequence->SetSkeleton(Skeleton);
				bResult = MovieSceneToolHelpers::ExportToAnimSequence(TempAnimSequence, ExportOptions, MovieScene, Player, SkeletalMeshComp, Template, RootToLocalTransform);
				if (bResult == false)
				{
					TempAnimSequence->MarkAsGarbage();
					if (OutActor)
					{
						World->DestroyActor(OutActor);
					}
					return false;
				}

				MovieScene->Modify();
				TArray<UMovieSceneTrack*> Tracks = MovieScene->FindTracks(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.BindingID, NAME_None);
				UMovieSceneControlRigParameterTrack* Track = nullptr;
				for (UMovieSceneTrack* AnyOleTrack : Tracks)
				{
					UMovieSceneControlRigParameterTrack* ValidTrack = Cast<UMovieSceneControlRigParameterTrack>(AnyOleTrack);
					if (ValidTrack)
					{
						Track = ValidTrack;
						Track->Modify();
						for (UMovieSceneSection* Section : Track->GetAllSections())
						{
							Section->SetIsActive(false);
						}
					}
				}
				if(Track == nullptr)
				{
					Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->AddTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.BindingID));
					{
						Track->Modify();
					}
				}

				if (Track)
				{
					FString ObjectName = InClass->GetName();
					ObjectName.RemoveFromEnd(TEXT("_C"));
					UControlRig* ControlRig = NewObject<UControlRig>(Track, InClass, FName(*ObjectName), RF_Transactional);
					FName OldEventString = FName(FString(TEXT("Inverse")));

					if (InClass != UFKControlRig::StaticClass() && !(ControlRig->SupportsEvent(FRigUnit_InverseExecution::EventName) || ControlRig->SupportsEvent(OldEventString)))
					{
						TempAnimSequence->MarkAsGarbage();
						MovieScene->RemoveTrack(*Track);
						if (OutActor)
						{
							World->DestroyActor(OutActor);
						}
						return false;
					}
					FControlRigEditMode* ControlRigEditMode = nullptr;
					if (WeakSequencer.IsValid())
					{
						ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
						if (!ControlRigEditMode)
						{
							GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);
							ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));

						}
						else
						{
							/* mz todo we don't unbind  will test more
							UControlRig* OldControlRig = ControlRigEditMode->GetControlRig(false);
							if (OldControlRig)
							{
								WeakSequencer.Pin()->ObjectImplicitlyRemoved(OldControlRig);
							}
							*/
						}
					}

					ControlRig->Modify();
					ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
					ControlRig->GetObjectBinding()->BindToObject(SkeletalMeshComp);
					ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ControlRig->GetObjectBinding()->GetBoundObject());
					ControlRig->Initialize();
					ControlRig->RequestInit();
					ControlRig->SetBoneInitialTransformsFromSkeletalMeshComponent(SkeletalMeshComp, true);
					ControlRig->Evaluate_AnyThread();

					bool bSequencerOwnsControlRig = true;
					UMovieSceneSection* NewSection = Track->CreateControlRigSection(0, ControlRig, bSequencerOwnsControlRig);
					UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(NewSection);

					//mz todo need to have multiple rigs with same class
					Track->SetTrackName(FName(*ObjectName));
					Track->SetDisplayName(FText::FromString(ObjectName));

					EMovieSceneKeyInterpolation DefaultInterpolation = EMovieSceneKeyInterpolation::SmartAuto;
					if (WeakSequencer.IsValid())
					{
						WeakSequencer.Pin()->EmptySelection();
						WeakSequencer.Pin()->SelectSection(NewSection);
						WeakSequencer.Pin()->ThrobSectionSelection();
						WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
						DefaultInterpolation = WeakSequencer.Pin()->GetKeyInterpolation();
					}

					ParamSection->LoadAnimSequenceIntoThisSection(TempAnimSequence, MovieScene, SkeletalMeshComp,
						bReduceKeys, Tolerance, bResetControls, FFrameNumber(0), DefaultInterpolation);

					//Turn Off Any Skeletal Animation Tracks
					UMovieSceneSkeletalAnimationTrack* SkelTrack = Cast<UMovieSceneSkeletalAnimationTrack>(MovieScene->FindTrack(UMovieSceneSkeletalAnimationTrack::StaticClass(), Binding.BindingID, NAME_None));
					if (SkelTrack)
					{
						SkelTrack->Modify();
						//can't just turn off the track so need to mute the sections
						const TArray<UMovieSceneSection*>& Sections = SkelTrack->GetAllSections();
						for (UMovieSceneSection* Section : Sections)
						{
							if (Section)
							{
								Section->TryModify();
								Section->SetIsActive(false);
							}
						}
					}
					//Finish Setup
					if (ControlRigEditMode)
					{
						ControlRigEditMode->AddControlRigObject(ControlRig, WeakSequencer.Pin());
					}

					TempAnimSequence->MarkAsGarbage();
					if (WeakSequencer.IsValid())
					{
						WeakSequencer.Pin()->ObjectImplicitlyAdded(ControlRig);
						WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
					}
					bResult = true;
				}
			}
		}
	}

	if (LevelPlayer)
	{
		LevelPlayer->Stop();
	}
	if (OutActor)
	{
		World->DestroyActor(OutActor);
	}
	return bResult;
}

bool UControlRigSequencerEditorLibrary::LoadAnimSequenceIntoControlRigSection(UMovieSceneSection* MovieSceneSection, UAnimSequence* AnimSequence, USkeletalMeshComponent* SkelMeshComp,
	FFrameNumber InStartFrame, EMovieSceneTimeUnit TimeUnit,bool bKeyReduce, float Tolerance, EMovieSceneKeyInterpolation Interpolation, bool bResetControls)
{
	if (MovieSceneSection == nullptr || AnimSequence == nullptr || SkelMeshComp == nullptr)
	{
		return false;
	}
	UMovieScene* MovieScene = MovieSceneSection->GetTypedOuter<UMovieScene>();
	if (MovieScene == nullptr)
	{
		return false;
	}
	if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(MovieSceneSection))
	{
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			InStartFrame = FFrameRate::TransformTime(FFrameTime(InStartFrame, 0), MovieScene->GetDisplayRate(),MovieScene->GetTickResolution()).RoundToFrame();
		}
		return Section->LoadAnimSequenceIntoThisSection(AnimSequence, MovieScene, SkelMeshComp, bKeyReduce, Tolerance, bResetControls, InStartFrame, Interpolation);
	}
	return false;
}

static bool LocalGetControlRigControlValues(IMovieScenePlayer* Player, UMovieSceneSequence* MovieSceneSequence, FMovieSceneSequenceIDRef Template, FMovieSceneSequenceTransform& RootToLocalTransform,
	UControlRig* ControlRig, const FName& ControlName, EMovieSceneTimeUnit TimeUnit,
	const TArray<FFrameNumber>& InFrames, TArray<FRigControlValue>& OutValues)
{
	if (Player == nullptr || MovieSceneSequence == nullptr || ControlRig == nullptr)
	{
		return false;
	}
	if (ControlRig->FindControl(ControlName) == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("Can not find Control %s"), *(ControlName.ToString()));
		return false;
	}
	if (UMovieScene* MovieScene = MovieSceneSequence->GetMovieScene())
	{

		FFrameRate TickResolution = MovieScene->GetTickResolution();
		OutValues.SetNum(InFrames.Num());
		for (int32 Index = 0; Index < InFrames.Num(); ++Index)
		{
			const FFrameNumber& FrameNumber = InFrames[Index];
			FFrameTime GlobalTime(FrameNumber);
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				GlobalTime = FFrameRate::TransformTime(GlobalTime, MovieScene->GetDisplayRate(), MovieScene->GetTickResolution());
			}
			GlobalTime = GlobalTime * RootToLocalTransform.InverseNoLooping();
			FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);

			Player->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context);
			ControlRig->Evaluate_AnyThread();
			OutValues[Index] = ControlRig->GetControlValue(ControlName);
		}
	}
	return true;
}

static bool GetControlRigValues(ISequencer* Sequencer, UControlRig* ControlRig, const FName& ControlName, EMovieSceneTimeUnit TimeUnit,
	const TArray<FFrameNumber>& Frames,  TArray<FRigControlValue>& OutValues)
{
	if (Sequencer->GetFocusedMovieSceneSequence())
	{
		FMovieSceneSequenceIDRef Template = Sequencer->GetFocusedTemplateID();
		FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();
		bool bGood = LocalGetControlRigControlValues(Sequencer, Sequencer->GetFocusedMovieSceneSequence(), Template, RootToLocalTransform,
			ControlRig, ControlName,TimeUnit, Frames, OutValues);
		Sequencer->RequestEvaluate();
		return bGood;
	}
	return false;
}

static bool GetControlRigValue(ISequencer* Sequencer, UControlRig* ControlRig, const FName& ControlName, EMovieSceneTimeUnit TimeUnit,
	const FFrameNumber Frame, FRigControlValue& OutValue)
{
	if (Sequencer->GetFocusedMovieSceneSequence())
	{
		TArray<FFrameNumber> Frames;
		Frames.Add(Frame);
		TArray<FRigControlValue> OutValues;
		FMovieSceneSequenceIDRef Template = Sequencer->GetFocusedTemplateID();
		FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();
		bool bVal = LocalGetControlRigControlValues(Sequencer, Sequencer->GetFocusedMovieSceneSequence(), Template, RootToLocalTransform,
			ControlRig, ControlName,TimeUnit, Frames, OutValues);
		if (bVal)
		{
			OutValue = OutValues[0];
		}
		return bVal;
	}
	return false;
}

static bool GetControlRigValues(UWorld* World, ULevelSequence* LevelSequence, UControlRig* ControlRig, const FName& ControlName, EMovieSceneTimeUnit TimeUnit,
	const TArray<FFrameNumber>& Frames, TArray<FRigControlValue>& OutValues)
{
	if (LevelSequence)
	{
		ALevelSequenceActor* OutActor = nullptr;
		FMovieSceneSequencePlaybackSettings Settings;
		FLevelSequenceCameraSettings CameraSettings;
		FMovieSceneSequenceIDRef Template = MovieSceneSequenceID::Root;
		FMovieSceneSequenceTransform RootToLocalTransform;
		ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, LevelSequence, Settings, OutActor);
		return LocalGetControlRigControlValues(Player, LevelSequence, Template, RootToLocalTransform,
			ControlRig, ControlName,TimeUnit, Frames, OutValues);

	}
	return false;
}


float UControlRigSequencerEditorLibrary::GetLocalControlRigFloat(ULevelSequence* LevelSequence,UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	float Value = 0.0f;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Float && Element->Settings.ControlType != ERigControlType::ScaleFloat)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frame, RigValue))
		{
			Value = RigValue.Get<float>();
		}
	}
	return Value;
}

TArray<float> UControlRigSequencerEditorLibrary::GetLocalControlRigFloats(ULevelSequence* LevelSequence,UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<float> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Float && Element->Settings.ControlType != ERigControlType::ScaleFloat)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}

		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				float Value = RigValue.Get<float>();
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigFloat(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, float Value, EMovieSceneTimeUnit TimeUnit, bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Float && Element->Settings.ControlType != ERigControlType::ScaleFloat)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<float>(ControlName, Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigFloats(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<float> Values,
	EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Float && Element->Settings.ControlType != ERigControlType::ScaleFloat)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			float  Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<float>(ControlName, Value, true, Context);
		}
	}
}


bool UControlRigSequencerEditorLibrary::GetLocalControlRigBool(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	bool Value = true;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Bool)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frame, RigValue))
		{
			Value = RigValue.Get<bool>();
		}
	}
	return Value;
}

TArray<bool> UControlRigSequencerEditorLibrary::GetLocalControlRigBools(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<bool> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Bool)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				bool Value = RigValue.Get<bool>();
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigBool(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, bool Value, EMovieSceneTimeUnit TimeUnit, bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Bool)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return;
			}
		}
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<bool>(ControlName, Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigBools(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
	const TArray<bool> Values, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Bool)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return;
			}
		}
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			bool Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<bool>(ControlName, Value, true, Context);
		}
	}
}

int32 UControlRigSequencerEditorLibrary::GetLocalControlRigInt(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	int32 Value = 0;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Integer)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frame, RigValue))
		{
			Value = RigValue.Get<int32>();
		}
	}
	return Value;
}

TArray<int32> UControlRigSequencerEditorLibrary::GetLocalControlRigInts(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<int32> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Integer)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit,Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				int32 Value = RigValue.Get<int32>();
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigInt(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, int32 Value, EMovieSceneTimeUnit TimeUnit,bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Integer)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<int32>(ControlName, Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigInts(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
	const TArray<int32> Values , EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Integer)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			int32 Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<int32>(ControlName, Value, true, Context);
		}
	}
}


FVector2D UControlRigSequencerEditorLibrary::GetLocalControlRigVector2D(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	FVector2D Value;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Vector2D)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frame, RigValue))
		{
			const FVector3f TempValue = RigValue.Get<FVector3f>(); 
			Value = FVector2D(TempValue.X, TempValue.Y);
		}
	}
	return Value;
}

TArray<FVector2D> UControlRigSequencerEditorLibrary::GetLocalControlRigVector2Ds(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<FVector2D> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Vector2D)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				const FVector3f TempValue = RigValue.Get<FVector3f>(); 
				Values.Add(FVector2D(TempValue.X, TempValue.Y));
			}
		}
	}
	return Values;

}

void UControlRigSequencerEditorLibrary::SetLocalControlRigVector2D(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector2D Value, EMovieSceneTimeUnit TimeUnit,bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Vector2D)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<FVector3f>(ControlName, FVector3f(Value.X, Value.Y, 0.f), true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigVector2Ds(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, 
	const TArray<FVector2D> Values, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Vector2D)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FVector2D Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FVector3f>(ControlName, FVector3f(Value.X, Value.Y, 0.f), true, Context);
		}
	}
}


FVector UControlRigSequencerEditorLibrary::GetLocalControlRigPosition(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	FVector Value;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Position)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frame, RigValue))
		{
			Value = (FVector)RigValue.Get<FVector3f>();
		}
	}
	return Value;
}

TArray<FVector> UControlRigSequencerEditorLibrary::GetLocalControlRigPositions(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<FVector> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Position)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				FVector Value = (FVector)RigValue.Get<FVector3f>();
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigPosition(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector Value, EMovieSceneTimeUnit TimeUnit,bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Position)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigPositions(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, 
	const TArray<FVector> Values, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Position)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FVector Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value, true, Context);
		}
	}
}


FRotator UControlRigSequencerEditorLibrary::GetLocalControlRigRotator(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	FRotator Value;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Rotator)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frame, RigValue))
		{
			Value = FRotator::MakeFromEuler((FVector)RigValue.Get<FVector3f>());
		}
	}
	return Value;
}

TArray<FRotator> UControlRigSequencerEditorLibrary::GetLocalControlRigRotators(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<FRotator> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Rotator)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				FRotator Value = FRotator::MakeFromEuler((FVector)RigValue.Get<FVector3f>());
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigRotator(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FRotator Value, EMovieSceneTimeUnit TimeUnit, bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Rotator)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value.Euler(), true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigRotators(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
	const TArray<FRotator> Values, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Rotator)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FRotator Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value.Euler(), true, Context);
		}
	}
}


FVector UControlRigSequencerEditorLibrary::GetLocalControlRigScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,  EMovieSceneTimeUnit TimeUnit)
{
	FVector Value;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Scale)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frame, RigValue))
		{
			Value = (FVector)RigValue.Get<FVector3f>();
		}
	}
	return Value;
}

TArray<FVector>UControlRigSequencerEditorLibrary::GetLocalControlRigScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<FVector> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Scale)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				FVector Value = (FVector)RigValue.Get<FVector3f>();
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector Value, EMovieSceneTimeUnit TimeUnit, bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Scale)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, 
	const TArray<FVector> Values, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Scale)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FVector Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value, true, Context);
		}
	}
}


FEulerTransform UControlRigSequencerEditorLibrary::GetLocalControlRigEulerTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	FEulerTransform Value = FEulerTransform::Identity;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::EulerTransform)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frame, RigValue))
		{
			Value = RigValue.Get<FRigControlValue::FEulerTransform_Float>().ToTransform();
		}
	}
	return Value;
}

TArray<FEulerTransform> UControlRigSequencerEditorLibrary::GetLocalControlRigEulerTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<FEulerTransform> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::EulerTransform)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				FEulerTransform Value = RigValue.Get<FRigControlValue::FEulerTransform_Float>().ToTransform();
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigEulerTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FEulerTransform Value,
	EMovieSceneTimeUnit TimeUnit, bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::EulerTransform)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		if (FRigControlElement* ControlElement = ControlRig->FindControl(ControlName))
		{
			FVector EulerAngle(Value.Rotation.Roll, Value.Rotation.Pitch, Value.Rotation.Yaw);
			ControlRig->GetHierarchy()->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
		}
		ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigEulerTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
	const TArray<FEulerTransform> Values, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::EulerTransform)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FEulerTransform Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));

			if (FRigControlElement* ControlElement = ControlRig->FindControl(ControlName))
			{
				FVector EulerAngle(Value.Rotation.Roll, Value.Rotation.Pitch, Value.Rotation.Yaw);
				ControlRig->GetHierarchy()->SetControlSpecifiedEulerAngle(ControlElement, EulerAngle);
			}
			ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Value, true, Context);
		}
	}
}


FTransformNoScale UControlRigSequencerEditorLibrary::GetLocalControlRigTransformNoScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	FTransformNoScale Value = FTransformNoScale::Identity;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::TransformNoScale)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frame, RigValue))
		{
			Value = RigValue.Get<FRigControlValue::FTransformNoScale_Float>().ToTransform();
		}
	}
	return Value;
}

TArray<FTransformNoScale> UControlRigSequencerEditorLibrary::GetLocalControlRigTransformNoScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<FTransformNoScale> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::TransformNoScale)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName,TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				FTransformNoScale Value = RigValue.Get<FRigControlValue::FTransformNoScale_Float>().ToTransform();
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigTransformNoScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FTransformNoScale Value,
	EMovieSceneTimeUnit TimeUnit,bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::TransformNoScale)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlName, Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigTransformNoScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, 
	const TArray<FTransformNoScale> Values, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::TransformNoScale)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FTransformNoScale Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlName, Value, true, Context);
		}
	}
}


FTransform UControlRigSequencerEditorLibrary::GetLocalControlRigTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, EMovieSceneTimeUnit TimeUnit)
{
	FTransform Value = FTransform::Identity;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Transform)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Value;
			}
		}
		
		FRigControlValue RigValue;
		if (GetControlRigValue(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frame, RigValue))
		{
			Value = RigValue.Get<FRigControlValue::FTransform_Float>().ToTransform();
		}
	}
	return Value;
}

TArray<FTransform> UControlRigSequencerEditorLibrary::GetLocalControlRigTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, EMovieSceneTimeUnit TimeUnit)
{
	TArray<FTransform> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Transform)
			{
				UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
				return Values;
			}
		}
		TArray<FRigControlValue> RigValues;
		if (GetControlRigValues(WeakSequencer.Pin().Get(), ControlRig, ControlName, TimeUnit, Frames, RigValues))
		{
			Values.Reserve(RigValues.Num());
			for (const FRigControlValue& RigValue : RigValues)
			{
				FTransform Value = RigValue.Get<FRigControlValue::FTransform_Float>().ToTransform();
				Values.Add(Value);
			}
		}
	}
	return Values;
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FTransform Value,
	EMovieSceneTimeUnit TimeUnit,bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Transform)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ControlName, Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, 
	const TArray<FTransform> Values, EMovieSceneTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Transform)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			FFrameNumber Frame = Frames[Index];
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FTransform Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ControlName, Value, true, Context);
		}
	}
}

bool UControlRigSequencerEditorLibrary::ImportFBXToControlRigTrack(UWorld* World, ULevelSequence* Sequence, UMovieSceneControlRigParameterTrack* InTrack, UMovieSceneControlRigParameterSection* InSection,
	const TArray<FString>& ControlRigNames,
	UMovieSceneUserImportFBXControlRigSettings* ImportFBXControlRigSettings,
	const FString& ImportFilename)
{

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene || MovieScene->IsReadOnly() || !InTrack)
	{
		return false;
	}

	bool bValid = false;
	ALevelSequenceActor* OutActor;
	FMovieSceneSequencePlaybackSettings Settings;
	FLevelSequenceCameraSettings CameraSettings;
	ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, Sequence, Settings, OutActor);

	INodeAndChannelMappings* ChannelMapping = Cast<INodeAndChannelMappings>(InTrack);
	if (ChannelMapping)
	{
		TArray<FRigControlFBXNodeAndChannels>* NodeAndChannels = ChannelMapping->GetNodeAndChannelMappings(InSection);
		TArray<FName> SelectedControls;
		for (const FString& StringName : ControlRigNames)
		{
			FName Name(*StringName);
			SelectedControls.Add(Name);
		}

		bValid = MovieSceneToolHelpers::ImportFBXIntoControlRigChannels(MovieScene, ImportFilename, ImportFBXControlRigSettings,
			NodeAndChannels, SelectedControls, MovieScene->GetTickResolution());
		if (NodeAndChannels)
		{
			delete NodeAndChannels;
		}
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::ExportFBXFromControlRigSection(ULevelSequence* Sequence, const UMovieSceneControlRigParameterSection* Section, const UMovieSceneUserExportFBXControlRigSettings* ExportFBXControlRigSettings)
{
	if (!Sequence || !ExportFBXControlRigSettings ||
		!Section || !Section->GetControlRig() ||
		!Sequence->GetMovieScene() || Sequence->GetMovieScene()->IsReadOnly())
	{
		return false;
	}

	TArray<FName> SelectedControls;
	if (UMovieSceneControlRigParameterTrack* Track = Section->GetTypedOuter<UMovieSceneControlRigParameterTrack>())
	{
		Track->GetSelectedNodes(SelectedControls);
	}

	FMovieSceneSequenceTransform RootToLocalTransform;
	if (IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Sequence, false))
	{
		if (const ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor))
		{
			if (const TSharedPtr<ISequencer> Sequencer = LevelSequenceEditor->GetSequencer())
			{
				RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();
			}
		}
	}
	
	return  MovieSceneToolHelpers::ExportFBXFromControlRigChannels(Section, ExportFBXControlRigSettings, SelectedControls, RootToLocalTransform);
}

bool UControlRigSequencerEditorLibrary::CollapseControlRigAnimLayersWithSettings(ULevelSequence* InSequence, UMovieSceneControlRigParameterTrack* InTrack, const FBakingAnimationKeySettings& InSettings)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	bool bValid = false;
	if (WeakSequencer.IsValid() && InTrack)
	{
		TSharedPtr<ISequencer>  SequencerPtr = WeakSequencer.Pin();
		bValid = FControlRigParameterTrackEditor::CollapseAllLayers(SequencerPtr, InTrack, InSettings);
		
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::CollapseControlRigAnimLayers(ULevelSequence* LevelSequence, UMovieSceneControlRigParameterTrack* InTrack, bool bKeyReduce, float Tolerance)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	bool bValid = false;

	if (WeakSequencer.IsValid() && InTrack)
	{
		TSharedPtr<ISequencer>  SequencerPtr = WeakSequencer.Pin();
		FBakingAnimationKeySettings CollapseControlsSettings;
		const FFrameRate TickResolution = SequencerPtr->GetFocusedTickResolution();
		const FFrameTime FrameTime = SequencerPtr->GetLocalTime().ConvertTo(TickResolution);
		FFrameNumber CurrentTime = FrameTime.GetFrame();

		TRange<FFrameNumber> Range = SequencerPtr->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();

		CollapseControlsSettings.StartFrame = Range.GetLowerBoundValue();
		CollapseControlsSettings.EndFrame = Range.GetUpperBoundValue();
		bValid = FControlRigParameterTrackEditor::CollapseAllLayers(SequencerPtr,InTrack, CollapseControlsSettings);	
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::SetControlRigSpace(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const FRigElementKey& InSpaceKey, FFrameNumber InTime, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	bool bValid = false;

	if (WeakSequencer.IsValid() && ControlRig && ControlName != NAME_None)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
			FScopedTransaction Transaction(LOCTEXT("KeyControlRigSpace", "Key Control Rig Space"));
			FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(ControlRig, ControlName, Sequencer.Get(), true /*bCreateIfNeeded*/);
			if (SpaceChannelAndSection.SpaceChannel)
			{
				if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
				{
					InTime = FFrameRate::TransformTime(FFrameTime(InTime, 0), LevelSequence->GetMovieScene()->GetDisplayRate(), LevelSequence->GetMovieScene()->GetTickResolution()).RoundToFrame();
				}
				FKeyHandle Handle = FControlRigSpaceChannelHelpers::SequencerKeyControlRigSpaceChannel(ControlRig, Sequencer.Get(), SpaceChannelAndSection.SpaceChannel, SpaceChannelAndSection.SectionToKey, InTime, ControlRig->GetHierarchy(), Element->GetKey(), InSpaceKey);
				bValid = Handle != FKeyHandle::Invalid();
			}
			else
			{
				UE_LOG(LogControlRig, Error, TEXT("Can not find Space Channel"));
				return false;
			}
		}
		else
		{
			UE_LOG(LogControlRig, Error, TEXT("Can not find Control with that Name"));
			return false;
		}
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::BakeControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, const TArray<FName>& InControlNames, FRigSpacePickerBakeSettings InSettings, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	bool bValid = false;

	if (WeakSequencer.IsValid() && InControlRig && InControlNames.Num() > 0)
	{
		TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
		const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		const FFrameRate& FrameRate = Sequencer->GetFocusedDisplayRate();
		FFrameNumber FrameRateInFrameNumber = TickResolution.AsFrameNumber(FrameRate.AsInterval());
		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			InSettings.Settings.StartFrame = FFrameRate::TransformTime(FFrameTime(InSettings.Settings.StartFrame, 0), FrameRate, TickResolution).RoundToFrame();
			InSettings.Settings.EndFrame = FFrameRate::TransformTime(FFrameTime(InSettings.Settings.EndFrame, 0), FrameRate, TickResolution).RoundToFrame();
		}

		FScopedTransaction Transaction(LOCTEXT("BakeControlToSpace", "Bake Control In Space"));
		for (const FName& ControlName : InControlNames)
		{
			if (FRigControlElement* Element = InControlRig->FindControl(ControlName))
			{
				FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(InControlRig, ControlName, Sequencer.Get(), false /*bCreateIfNeeded*/);
				if (SpaceChannelAndSection.SpaceChannel)
				{
					FControlRigSpaceChannelHelpers::SequencerBakeControlInSpace(InControlRig, Sequencer.Get(), SpaceChannelAndSection.SpaceChannel, SpaceChannelAndSection.SectionToKey,
						InControlRig->GetHierarchy(), Element->GetKey(), InSettings);
				}
			}
		}
		bValid = true;
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::DeleteControlRigSpace(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber InTime, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	bool bValid = false;

	if (WeakSequencer.IsValid() && ControlRig && ControlName != NAME_None)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
			FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(ControlRig, ControlName, Sequencer.Get(), false /*bCreateIfNeeded*/);
			if (SpaceChannelAndSection.SpaceChannel)
			{

				if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
				{
					InTime = FFrameRate::TransformTime(FFrameTime(InTime, 0), LevelSequence->GetMovieScene()->GetDisplayRate(), LevelSequence->GetMovieScene()->GetTickResolution()).RoundToFrame();
				}
				UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(SpaceChannelAndSection.SectionToKey);


				TArray<FFrameNumber> OurKeyTimes;
				TArray<FKeyHandle> OurKeyHandles;
				TRange<FFrameNumber> CurrentFrameRange;
				CurrentFrameRange.SetLowerBound(TRangeBound<FFrameNumber>(InTime));
				CurrentFrameRange.SetUpperBound(TRangeBound<FFrameNumber>(InTime));
				TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey> ChannelInterface = SpaceChannelAndSection.SpaceChannel->GetData();
				ChannelInterface.GetKeys(CurrentFrameRange, &OurKeyTimes, &OurKeyHandles);
				if (OurKeyHandles.Num() > 0)
				{
					FScopedTransaction DeleteKeysTransaction(LOCTEXT("DeleteSpaceChannelKeys_Transaction", "Delete Space Channel Keys"));
					ParamSection->TryModify();
					SpaceChannelAndSection.SpaceChannel->DeleteKeys(OurKeyHandles);
					WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
					bValid = true;
				}
				else
				{
					UE_LOG(LogControlRig, Error, TEXT("No Keys To Delete At that Time"));
					return false;
				}
			}
			else
			{
				UE_LOG(LogControlRig, Error, TEXT("Can not find Space Channel"));
				return false;
			}
		}
		else
		{
			UE_LOG(LogControlRig, Error, TEXT("Can not find Control with that Name"));
			return false;
		}
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::MoveControlRigSpace(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber InTime, FFrameNumber InNewTime, EMovieSceneTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	bool bValid = false;

	if (WeakSequencer.IsValid() && ControlRig && ControlName != NAME_None)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
			FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(ControlRig, ControlName, Sequencer.Get(), false /*bCreateIfNeeded*/);
			if (SpaceChannelAndSection.SpaceChannel)
			{

				if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
				{
					InTime = FFrameRate::TransformTime(FFrameTime(InTime, 0), LevelSequence->GetMovieScene()->GetDisplayRate(), LevelSequence->GetMovieScene()->GetTickResolution()).RoundToFrame();
					InNewTime = FFrameRate::TransformTime(FFrameTime(InNewTime, 0), LevelSequence->GetMovieScene()->GetDisplayRate(), LevelSequence->GetMovieScene()->GetTickResolution()).RoundToFrame();
				}
				UMovieSceneControlRigParameterSection* ParamSection = Cast<UMovieSceneControlRigParameterSection>(SpaceChannelAndSection.SectionToKey);

				TArray<FFrameNumber> OurKeyTimes;
				TArray<FKeyHandle> OurKeyHandles;
				TRange<FFrameNumber> CurrentFrameRange;
				CurrentFrameRange.SetLowerBound(TRangeBound<FFrameNumber>(InTime));
				CurrentFrameRange.SetUpperBound(TRangeBound<FFrameNumber>(InTime));
				TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey> ChannelInterface = SpaceChannelAndSection.SpaceChannel->GetData();
				ChannelInterface.GetKeys(CurrentFrameRange, &OurKeyTimes, &OurKeyHandles);
				if (OurKeyHandles.Num() > 0)
				{
					FScopedTransaction MoveKeys(LOCTEXT("MoveSpaceChannelKeys_Transaction", "Move Space Channel Keys"));
					ParamSection->TryModify();
					TArray<FFrameNumber> NewKeyTimes;
					NewKeyTimes.Add(InNewTime);
					SpaceChannelAndSection.SpaceChannel->SetKeyTimes(OurKeyHandles, NewKeyTimes);
					WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
					bValid = true;
				}
				else
				{
					UE_LOG(LogControlRig, Error, TEXT("No Key To Move At that Time"));
					return false;
				}
			}
			else
			{
				UE_LOG(LogControlRig, Error, TEXT("Can not find Space Channel"));
				return false;
			}
		}
		else
		{
			UE_LOG(LogControlRig, Error, TEXT("Can not find Control with that Name"));
			return false;
		}
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::RenameControlRigControlChannels(ULevelSequence* LevelSequence, UControlRig* ControlRig, const TArray<FName>& InOldControlNames, const TArray<FName>& InNewControlNames)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		UE_LOG(LogControlRig, Error, TEXT("LevelSequence and Control Rig must be valid"));
		return false;
	}
	if (InOldControlNames.Num() != InNewControlNames.Num() || InOldControlNames.Num() < 1)
	{
		UE_LOG(LogControlRig, Error, TEXT("Old and New Control Name arrays don't match in length"));
		return false;
	}
	for (const FName& NewName : InNewControlNames)
	{
		if (ControlRig->FindControl(NewName) == nullptr)
		{
			const FString StringName = NewName.ToString();
			UE_LOG(LogControlRig, Error, TEXT("Missing Control Name %s"), *(StringName));
			return false;
		}
	}
	bool bValid = false;


	UMovieScene* MovieScene = LevelSequence->GetMovieScene();
	if (MovieScene)
	{
		const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			TArray<UMovieSceneTrack*> Tracks = MovieScene->FindTracks(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None);
			for (UMovieSceneTrack* AnyOleTrack : Tracks)
			{
				UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(AnyOleTrack);
				if (Track && Track->GetControlRig() == ControlRig)
				{
					Track->Modify();
					bValid = true;
					for (int32 Index = 0; Index < InOldControlNames.Num(); ++Index)
					{
						Track->RenameParameterName(InOldControlNames[Index], InNewControlNames[Index]);
					}
				}
			}
		}
	}
	
	return bValid;
}

FRigElementKey UControlRigSequencerEditorLibrary::GetDefaultParentKey()
{
	return URigHierarchy::GetDefaultParentKey();
}

FRigElementKey UControlRigSequencerEditorLibrary::GetWorldSpaceReferenceKey()
{
	return URigHierarchy::GetWorldSpaceReferenceKey();
}

void UControlRigSequencerEditorLibrary::SetControlRigPriorityOrder(UMovieSceneTrack* InTrack, int32 PriorityOrder)
{
	UMovieSceneControlRigParameterTrack* ParameterTrack = Cast<UMovieSceneControlRigParameterTrack>(InTrack);
	if (!ParameterTrack)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call SetControlRigPriorityOrder without a UMovieSceneControlRigParameterTrack"), ELogVerbosity::Error);
		return;
	}

	ParameterTrack->SetPriorityOrder(PriorityOrder);
}

int32 UControlRigSequencerEditorLibrary::GetControlRigPriorityOrder(UMovieSceneTrack* InTrack)
{
	UMovieSceneControlRigParameterTrack* ParameterTrack = Cast<UMovieSceneControlRigParameterTrack>(InTrack);
	if (!ParameterTrack)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call SetControlRigPriorityOrder without a UMovieSceneControlRigParameterTrack"), ELogVerbosity::Error);
		return INDEX_NONE;
	}
	return 	ParameterTrack->GetPriorityOrder();

}

bool UControlRigSequencerEditorLibrary::GetControlsMask(UMovieSceneSection* InSection, FName ControlName)
{
	UMovieSceneControlRigParameterSection* ParameterSection = Cast<UMovieSceneControlRigParameterSection>(InSection);
	if (!ParameterSection)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call GetControlsMask without a UMovieSceneControlRigParameterSection"), ELogVerbosity::Error);
		return false;
	}
	
	UControlRig* ControlRig = Cast<UControlRig>(ParameterSection->GetControlRig());
	if (!ControlRig)
	{
		FFrame::KismetExecutionMessage(TEXT("Section does not have a control rig"), ELogVerbosity::Error);
		return false;
	}

	TArray<FRigControlElement*> Controls;
	ControlRig->GetControlsInOrder(Controls);
	int32 Index = 0;
	for (const FRigControlElement* RigControl : Controls)
	{
		if (RigControl->GetFName() == ControlName)
		{
			return ParameterSection->GetControlsMask(Index);
		}
		++Index;
	}

	FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Control Name ('%s') not found"), *ControlName.ToString()), ELogVerbosity::Error);
	return false;
}

void UControlRigSequencerEditorLibrary::SetControlsMask(UMovieSceneSection* InSection, const TArray<FName>& ControlNames, bool bVisible)
{
	UMovieSceneControlRigParameterSection* ParameterSection = Cast<UMovieSceneControlRigParameterSection>(InSection);
	if (!ParameterSection)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call SetControlsMask without a UMovieSceneControlRigParameterSection"), ELogVerbosity::Error);
		return;
	}

	UControlRig* ControlRig = Cast<UControlRig>(ParameterSection->GetControlRig());
	if (!ControlRig)
	{
		FFrame::KismetExecutionMessage(TEXT("Section does not have a control rig"), ELogVerbosity::Error);
		return;
	}

	ParameterSection->Modify();

	TArray<FRigControlElement*> Controls;
	ControlRig->GetControlsInOrder(Controls);
	int32 Index = 0;
	for (const FRigControlElement* RigControl : Controls)
	{
		if (ControlNames.Contains(RigControl->GetFName()))
		{
			ParameterSection->SetControlsMask(Index, bVisible);
		}
		++Index;
	}
}

void UControlRigSequencerEditorLibrary::ShowAllControls(UMovieSceneSection* InSection)
{
	UMovieSceneControlRigParameterSection* ParameterSection = Cast<UMovieSceneControlRigParameterSection>(InSection);
	if (!ParameterSection)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call ShowAllControls without a UMovieSceneControlRigParameterSection"), ELogVerbosity::Error);
		return;
	}

	ParameterSection->Modify();
	ParameterSection->FillControlsMask(true);
}

void UControlRigSequencerEditorLibrary::HideAllControls(UMovieSceneSection* InSection)
{
	UMovieSceneControlRigParameterSection* ParameterSection = Cast<UMovieSceneControlRigParameterSection>(InSection);
	if (!ParameterSection)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call HideAllControls without a UMovieSceneControlRigParameterSection"), ELogVerbosity::Error);
		return;
	}

	ParameterSection->Modify();
	ParameterSection->FillControlsMask(false);
}

bool UControlRigSequencerEditorLibrary::IsFKControlRig(UControlRig* InControlRig)
{
	return (InControlRig && InControlRig->IsA<UFKControlRig>());
}

bool UControlRigSequencerEditorLibrary::IsLayeredControlRig(UControlRig* InControlRig)
{
	return (InControlRig && InControlRig->IsAdditive());
}

bool UControlRigSequencerEditorLibrary::SetControlRigLayeredMode(UMovieSceneControlRigParameterTrack* InTrack, bool bSetIsLayered)
{
	if (!InTrack)
	{
		FFrame::KismetExecutionMessage(TEXT("Invalid track"), ELogVerbosity::Error);
		return false;
	}

	UControlRig* ControlRig = InTrack->GetControlRig();
	if (!ControlRig)
	{
		FFrame::KismetExecutionMessage(TEXT("Track does not have a control rig"), ELogVerbosity::Error);
		return false;
	}

	if (ControlRig->IsAdditive() == bSetIsLayered)
	{
		if(bSetIsLayered)
		{
			FFrame::KismetExecutionMessage(TEXT("Control rig is already in layered mode"), ELogVerbosity::Error);
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("Control rig is already in absolute mode"), ELogVerbosity::Error);
		}
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("ConvertToLayeredControlRig_Transaction", "Convert to Layered Control Rig"));
	InTrack->Modify();
	ControlRig->Modify();

	if (UFKControlRig* FKRig = Cast<UFKControlRig>(ControlRig))
	{
		FKRig->SetApplyMode(bSetIsLayered ? EControlRigFKRigExecuteMode::Additive : EControlRigFKRigExecuteMode::Replace);
	}
	else
	{
		ControlRig->ClearPoseBeforeBackwardsSolve();
		ControlRig->ResetControlValues();
		ControlRig->SetIsAdditive(bSetIsLayered);

		ControlRig->Evaluate_AnyThread();
	}

	FString ObjectName = ControlRig->GetClass()->GetName(); //GetDisplayNameText().ToString();
	ObjectName.RemoveFromEnd(TEXT("_C"));
	
	if (bSetIsLayered)
	{
		const FString AdditiveObjectName = ObjectName + TEXT(" (Layered)");
		InTrack->SetTrackName(FName(*ObjectName));
		InTrack->SetDisplayName(FText::FromString(AdditiveObjectName));
		InTrack->SetColorTint(UMovieSceneControlRigParameterTrack::LayeredRigTrackColor);
	}
	else
	{
		InTrack->SetTrackName(FName(*ObjectName));
		InTrack->SetDisplayName(FText::FromString(ObjectName));
		InTrack->SetColorTint(UMovieSceneControlRigParameterTrack::AbsoluteRigTrackColor);
	}

	FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
	if (ControlRigEditMode)
	{
		ControlRigEditMode->ZeroTransforms(false);
	}

	for (UMovieSceneSection* Section : InTrack->GetAllSections())
	{
		if (Section)
		{
			UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(Section);
			if (CRSection)
			{
				Section->Modify();
				CRSection->ClearAllParameters();
				CRSection->RecreateWithThisControlRig(CRSection->GetControlRig(), true);
			}
		}
	}

	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	if (WeakSequencer.IsValid())
	{
		WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	}

	return true;
}

EControlRigFKRigExecuteMode UControlRigSequencerEditorLibrary::GetFKControlRigApplyMode(UControlRig* InControlRig)
{
	EControlRigFKRigExecuteMode ApplyMode = EControlRigFKRigExecuteMode::Direct;
	if (UFKControlRig* FKRig = Cast<UFKControlRig>(Cast<UControlRig>(InControlRig)))
	{
		ApplyMode = FKRig->GetApplyMode();
	}
	return ApplyMode;
}

bool UControlRigSequencerEditorLibrary::SetControlRigApplyMode(UControlRig* InControlRig, EControlRigFKRigExecuteMode InApplyMode)
{
	if (UFKControlRig* FKRig = Cast<UFKControlRig>(Cast<UControlRig>(InControlRig)))
	{
		FKRig->SetApplyMode(InApplyMode);
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE

