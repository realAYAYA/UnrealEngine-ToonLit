// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequencerEditorLibrary.h"
#include "LevelSequence.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
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
						BindingProxy.ControlRig = Track->GetControlRig();
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

static UMovieSceneControlRigParameterTrack* AddControlRig(ULevelSequence* LevelSequence,const UClass* InClass, UObject* BoundActor, FGuid ObjectBinding, UControlRig* InExistingControlRig)
{
	FSlateApplication::Get().DismissAllMenus();

	if (InClass && InClass->IsChildOf(UControlRig::StaticClass()) && LevelSequence && LevelSequence->GetMovieScene())
	{
		UMovieScene* OwnerMovieScene = LevelSequence->GetMovieScene();
		TSharedPtr<ISequencer> SharedSequencer = GetSequencerFromAsset();
		ISequencer* Sequencer = nullptr; // will be valid  if we have a ISequencer AND it's focused.
		if (SharedSequencer.IsValid() && SharedSequencer->GetFocusedMovieSceneSequence() == LevelSequence)
		{
			Sequencer = SharedSequencer.Get();
		}
		LevelSequence->Modify();
		OwnerMovieScene->Modify();
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
			ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
			ControlRig->GetObjectBinding()->BindToObject(BoundActor);
			ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ControlRig->GetObjectBinding()->GetBoundObject());
			ControlRig->Initialize();
			ControlRig->Evaluate_AnyThread();


			Track->Modify();
			UMovieSceneSection* NewSection = Track->CreateControlRigSection(0, ControlRig, bSequencerOwnsControlRig);
			NewSection->Modify();

			//mz todo need to have multiple rigs with same class
			Track->SetTrackName(FName(*ObjectName));
			Track->SetDisplayName(FText::FromString(ObjectName));

			if (Sequencer)
			{
				Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
				Sequencer->EmptySelection();
				Sequencer->SelectSection(NewSection);
				Sequencer->ThrobSectionSelection();
				Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
				Sequencer->ObjectImplicitlyAdded(ControlRig);
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
	}
	return nullptr;
}

UMovieSceneTrack* UControlRigSequencerEditorLibrary::FindOrCreateControlRigTrack(UWorld* World, ULevelSequence* LevelSequence, const UClass* ControlRigClass, const FMovieSceneBindingProxy& InBinding)
{
	UMovieScene* MovieScene = InBinding.Sequence ? InBinding.Sequence->GetMovieScene() : nullptr;
	UMovieSceneTrack* BaseTrack = nullptr;
	if (LevelSequence && MovieScene && InBinding.BindingID.IsValid())
	{
		const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			if (Binding.GetObjectGuid() == InBinding.BindingID)
			{
				TArray<UMovieSceneTrack*> Tracks = MovieScene->FindTracks(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None);
				for (UMovieSceneTrack* AnyOleTrack : Tracks)
				{
					UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(AnyOleTrack);
					if (Track && Track->GetControlRig() && Track->GetControlRig()->GetClass() == ControlRigClass)
					{
						return Track;
					}
				}

				TArray<UObject*, TInlineAllocator<1>> Result;
				UObject* Context = nullptr;
				ALevelSequenceActor* OutActor = nullptr;
				ULevelSequencePlayer* OutPlayer = nullptr;
				Result = GetBoundObjects(World, LevelSequence, InBinding, &OutPlayer, &OutActor);
				if (Result.Num() > 0 && Result[0])
				{
					UObject* BoundObject = Result[0];
					USkeleton* Skeleton = nullptr;
					USkeletalMeshComponent* SkeletalMeshComponent = nullptr;
					AcquireSkeletonAndSkelMeshCompFromObject(BoundObject, &Skeleton, &SkeletalMeshComponent);

					UControlRig* ControlRig = nullptr;
					if (Skeleton && SkeletalMeshComponent)
					{
						UMovieSceneControlRigParameterTrack* Track = AddControlRig(LevelSequence, ControlRigClass, SkeletalMeshComponent, InBinding.BindingID, nullptr);

						if (Track)
						{
							BaseTrack = Track;								
						}
					}
				}

				if (OutPlayer)
				{
					OutPlayer->Stop();
				}

				if (OutActor)
				{
					World->DestroyActor(OutActor);
				}
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
				if (UControlRig* CR = ControlRigComponent->GetControlRig())
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
						GoodTrack = AddControlRig(LevelSequence, CR->GetClass(), BoundActor, InBinding.BindingID, CR);
					}
					Tracks.Add(GoodTrack);
				}
			}
		}
	}

	if (OutPlayer)
	{
		OutPlayer->Stop();
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

bool UControlRigSequencerEditorLibrary::BakeConstraint(UWorld* World, UTickableConstraint* Constraint, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
{
	if (!World)
	{
		UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need Valid World"));
		return false;
	}
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			RealFramesToBake[Index++] = Frame;
		} 
		if (RealFramesToBake.Num() > 0)
		{
			FramesToBake = RealFramesToBake;
		}
		FConstraintBaker::Bake(World, TransformConstraint, Sequencer, FramesToBake);
	}
	else
	{
		UE_LOG(LogControlRig, Error, TEXT("BakeConstraint: Need Valid Constraint"));
		return false;
	}
	return true;
}


bool UControlRigSequencerEditorLibrary::SnapControlRig(ULevelSequence* LevelSequence, FFrameNumber StartFrame, FFrameNumber EndFrame, const FControlRigSnapperSelection& ChildrenToSnap,
	const FControlRigSnapperSelection& ParentToSnap, const UControlRigSnapSettings* SnapSettings, ESequenceTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || LevelSequence->GetMovieScene() == nullptr)
	{
		return false;
	}
	FControlRigSnapper Snapper;
	if (TimeUnit == ESequenceTimeUnit::DisplayRate)
	{
		StartFrame = FFrameRate::TransformTime(FFrameTime(StartFrame, 0), LevelSequence->GetMovieScene()->GetDisplayRate(), LevelSequence->GetMovieScene()->GetTickResolution()).RoundToFrame();
		EndFrame = FFrameRate::TransformTime(FFrameTime(EndFrame, 0), LevelSequence->GetMovieScene()->GetDisplayRate(), LevelSequence->GetMovieScene()->GetTickResolution()).RoundToFrame();

	}
	return Snapper.SnapIt(StartFrame, EndFrame, ChildrenToSnap, ParentToSnap,SnapSettings);
}

FTransform UControlRigSequencerEditorLibrary::GetActorWorldTransform(ULevelSequence* LevelSequence, AActor* Actor, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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
static void ConvertFramesToTickResolution(UMovieScene* MovieScene, const TArray<FFrameNumber>& InFrames, ESequenceTimeUnit TimeUnit, TArray<FFrameNumber>& OutFrames)
{
	OutFrames.SetNum(InFrames.Num());
	int32 Index = 0;
	for (const FFrameNumber& Frame : InFrames)
	{
		FFrameTime FrameTime(Frame);
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
		{
			FrameTime = FFrameRate::TransformTime(FrameTime, MovieScene->GetDisplayRate(), MovieScene->GetTickResolution());
		}
		OutFrames[Index++] = FrameTime.RoundToFrame();
	}
}

TArray<FTransform> UControlRigSequencerEditorLibrary::GetActorWorldTransforms(ULevelSequence* LevelSequence,AActor* Actor, const TArray<FFrameNumber>& InFrames, ESequenceTimeUnit TimeUnit)
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
	ESequenceTimeUnit TimeUnit, FName SocketName)
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
	ESequenceTimeUnit TimeUnit, FName SocketName)
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
	ESequenceTimeUnit TimeUnit)
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
	ESequenceTimeUnit TimeUnit)
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
	const TArray<FTransform>& WorldTransforms, ESequenceTimeUnit TimeUnit)
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
						ControlRig->SetControlGlobalTransform(ControlName, GlobalTransform, true, Context);
					}
				}
			}
		}
	}
}

void UControlRigSequencerEditorLibrary::SetControlRigWorldTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, const FTransform& WorldTransform,
	ESequenceTimeUnit TimeUnit, bool bSetKey)
{
	EControlRigSetKey SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
	TArray<FFrameNumber> Frames;
	Frames.Add(Frame);
	TArray<FTransform> WorldTransforms;
	WorldTransforms.Add(WorldTransform);

	LocalSetControlRigWorldTransforms(LevelSequence, ControlRig, ControlName, SetKey, Frames, WorldTransforms, TimeUnit);

}

void UControlRigSequencerEditorLibrary::SetControlRigWorldTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, const TArray<FTransform>& WorldTransforms,
	ESequenceTimeUnit TimeUnit)
{
	LocalSetControlRigWorldTransforms(LevelSequence, ControlRig, ControlName, EControlRigSetKey::Always, Frames, WorldTransforms,TimeUnit);
}

bool UControlRigSequencerEditorLibrary::BakeToControlRig(UWorld* World, ULevelSequence* LevelSequence, UClass* InClass, UAnimSeqExportOption* ExportOptions, bool bReduceKeys, float Tolerance,
	const FMovieSceneBindingProxy& Binding)
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

					if (WeakSequencer.IsValid())
					{
						WeakSequencer.Pin()->EmptySelection();
						WeakSequencer.Pin()->SelectSection(NewSection);
						WeakSequencer.Pin()->ThrobSectionSelection();
						WeakSequencer.Pin()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
					}
					ParamSection->LoadAnimSequenceIntoThisSection(TempAnimSequence, MovieScene, SkeletalMeshComp,
						bReduceKeys, Tolerance);

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
	FFrameNumber InStartFrame, ESequenceTimeUnit TimeUnit,bool bKeyReduce, float Tolerance)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
		{
			InStartFrame = FFrameRate::TransformTime(FFrameTime(InStartFrame, 0), MovieScene->GetDisplayRate(),MovieScene->GetTickResolution()).RoundToFrame();
		}
		return Section->LoadAnimSequenceIntoThisSection(AnimSequence, MovieScene, SkelMeshComp, bKeyReduce, Tolerance, InStartFrame);
	}
	return false;
}

static bool LocalGetControlRigControlValues(IMovieScenePlayer* Player, UMovieSceneSequence* MovieSceneSequence, FMovieSceneSequenceIDRef Template, FMovieSceneSequenceTransform& RootToLocalTransform,
	UControlRig* ControlRig, const FName& ControlName, ESequenceTimeUnit TimeUnit,
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				GlobalTime = FFrameRate::TransformTime(GlobalTime, MovieScene->GetDisplayRate(), MovieScene->GetTickResolution());
			}
			GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly();
			FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);

			Player->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *Player);
			ControlRig->Evaluate_AnyThread();
			OutValues[Index] = ControlRig->GetControlValue(ControlName);
		}
	}
	return true;
}

static bool GetControlRigValues(ISequencer* Sequencer, UControlRig* ControlRig, const FName& ControlName, ESequenceTimeUnit TimeUnit,
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

static bool GetControlRigValue(ISequencer* Sequencer, UControlRig* ControlRig, const FName& ControlName, ESequenceTimeUnit TimeUnit,
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

static bool GetControlRigValues(UWorld* World, ULevelSequence* LevelSequence, UControlRig* ControlRig, const FName& ControlName, ESequenceTimeUnit TimeUnit,
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


float UControlRigSequencerEditorLibrary::GetLocalControlRigFloat(ULevelSequence* LevelSequence,UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
{
	float Value = 0.0f;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Float)
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

TArray<float> UControlRigSequencerEditorLibrary::GetLocalControlRigFloats(ULevelSequence* LevelSequence,UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
{
	TArray<float> Values;
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();

	if (WeakSequencer.IsValid() && ControlRig)
	{
		if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
		{
			if (Element->Settings.ControlType != ERigControlType::Float)
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

void UControlRigSequencerEditorLibrary::SetLocalControlRigFloat(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, float Value, ESequenceTimeUnit TimeUnit, bool bSetKey)
{
	if (LevelSequence == nullptr || ControlRig == nullptr)
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Float)
		{
			UE_LOG(LogControlRig, Error, TEXT("Control Rig Wrong Type"));
			return;
		}
	}
	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	ESequenceTimeUnit TimeUnit)
{
	if (LevelSequence == nullptr || ControlRig == nullptr || (Frames.Num() != Values.Num()))
	{
		return;
	}
	if (FRigControlElement* Element = ControlRig->FindControl(ControlName))
	{
		if (Element->Settings.ControlType != ERigControlType::Float)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			float  Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<float>(ControlName, Value, true, Context);
		}
	}
}


bool UControlRigSequencerEditorLibrary::GetLocalControlRigBool(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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

TArray<bool> UControlRigSequencerEditorLibrary::GetLocalControlRigBools(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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

void UControlRigSequencerEditorLibrary::SetLocalControlRigBool(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, bool Value, ESequenceTimeUnit TimeUnit, bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	const TArray<bool> Values, ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			bool Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<bool>(ControlName, Value, true, Context);
		}
	}
}

int32 UControlRigSequencerEditorLibrary::GetLocalControlRigInt(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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

TArray<int32> UControlRigSequencerEditorLibrary::GetLocalControlRigInts(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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

void UControlRigSequencerEditorLibrary::SetLocalControlRigInt(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, int32 Value, ESequenceTimeUnit TimeUnit,bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	const TArray<int32> Values , ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			int32 Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<int32>(ControlName, Value, true, Context);
		}
	}
}


FVector2D UControlRigSequencerEditorLibrary::GetLocalControlRigVector2D(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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

TArray<FVector2D> UControlRigSequencerEditorLibrary::GetLocalControlRigVector2Ds(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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

void UControlRigSequencerEditorLibrary::SetLocalControlRigVector2D(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector2D Value, ESequenceTimeUnit TimeUnit,bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	const TArray<FVector2D> Values, ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FVector2D Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FVector3f>(ControlName, FVector3f(Value.X, Value.Y, 0.f), true, Context);
		}
	}
}


FVector UControlRigSequencerEditorLibrary::GetLocalControlRigPosition(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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

TArray<FVector> UControlRigSequencerEditorLibrary::GetLocalControlRigPositions(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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

void UControlRigSequencerEditorLibrary::SetLocalControlRigPosition(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector Value, ESequenceTimeUnit TimeUnit,bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	const TArray<FVector> Values, ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FVector Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value, true, Context);
		}
	}
}


FRotator UControlRigSequencerEditorLibrary::GetLocalControlRigRotator(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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

TArray<FRotator> UControlRigSequencerEditorLibrary::GetLocalControlRigRotators(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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

void UControlRigSequencerEditorLibrary::SetLocalControlRigRotator(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FRotator Value, ESequenceTimeUnit TimeUnit, bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	const TArray<FRotator> Values, ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FRotator Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value.Euler(), true, Context);
		}
	}
}


FVector UControlRigSequencerEditorLibrary::GetLocalControlRigScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame,  ESequenceTimeUnit TimeUnit)
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

TArray<FVector>UControlRigSequencerEditorLibrary::GetLocalControlRigScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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

void UControlRigSequencerEditorLibrary::SetLocalControlRigScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, FVector Value, ESequenceTimeUnit TimeUnit, bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	const TArray<FVector> Values, ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FVector Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Value, true, Context);
		}
	}
}


FEulerTransform UControlRigSequencerEditorLibrary::GetLocalControlRigEulerTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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

TArray<FEulerTransform> UControlRigSequencerEditorLibrary::GetLocalControlRigEulerTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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
	ESequenceTimeUnit TimeUnit, bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
		{
			Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
		}
		FRigControlModifiedContext Context;
		Context.SetKey = bSetKey ? EControlRigSetKey::Always : EControlRigSetKey::DoNotCare;
		Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
		ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Value, true, Context);
	}
}

void UControlRigSequencerEditorLibrary::SetLocalControlRigEulerTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames,
	const TArray<FEulerTransform> Values, ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FEulerTransform Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Value, true, Context);
		}
	}
}


FTransformNoScale UControlRigSequencerEditorLibrary::GetLocalControlRigTransformNoScale(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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

TArray<FTransformNoScale> UControlRigSequencerEditorLibrary::GetLocalControlRigTransformNoScales(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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
	ESequenceTimeUnit TimeUnit,bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	const TArray<FTransformNoScale> Values, ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
			{
				Frame = FFrameRate::TransformTime(FFrameTime(Frame, 0), MovieScene->GetDisplayRate(), MovieScene->GetTickResolution()).RoundToFrame();
			}
			FTransformNoScale Value = Values[Index];
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlName, Value, true, Context);
		}
	}
}


FTransform UControlRigSequencerEditorLibrary::GetLocalControlRigTransform(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber Frame, ESequenceTimeUnit TimeUnit)
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

TArray<FTransform> UControlRigSequencerEditorLibrary::GetLocalControlRigTransforms(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const TArray<FFrameNumber>& Frames, ESequenceTimeUnit TimeUnit)
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
	ESequenceTimeUnit TimeUnit,bool bSetKey)
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
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
	const TArray<FTransform> Values, ESequenceTimeUnit TimeUnit)
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
			if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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
		TArray<FFBXNodeAndChannels>* NodeAndChannels = ChannelMapping->GetNodeAndChannelMappings(InSection);
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

bool UControlRigSequencerEditorLibrary::CollapseControlRigAnimLayers(ULevelSequence* LevelSequence, UMovieSceneControlRigParameterTrack* InTrack, bool bKeyReduce, float Tolerance)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	bool bValid = false;

	if (WeakSequencer.IsValid() && InTrack)
	{
		TArray<UMovieSceneSection*> Sections = InTrack->GetAllSections();
		if (Sections.Num() > 0)
		{
			TSharedPtr<ISequencer>  SequencerPtr = WeakSequencer.Pin();
			UMovieSceneControlRigParameterSection* ParameterSection = Cast<UMovieSceneControlRigParameterSection>(Sections[0]);
			bValid = FControlRigParameterTrackEditor::CollapseAllLayers(SequencerPtr,InTrack, ParameterSection, bKeyReduce, Tolerance);
		}
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::SetControlRigSpace(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, const FRigElementKey& InSpaceKey, FFrameNumber InTime, ESequenceTimeUnit TimeUnit)
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
				if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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

bool UControlRigSequencerEditorLibrary::BakeControlRigSpace(ULevelSequence* InSequence, UControlRig* InControlRig, const TArray<FName>& InControlNames, FRigSpacePickerBakeSettings InSettings, ESequenceTimeUnit TimeUnit)
{
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerFromAsset();
	bool bValid = false;

	if (WeakSequencer.IsValid() && InControlRig && InControlNames.Num() > 0)
	{
		TSharedPtr<ISequencer>  Sequencer = WeakSequencer.Pin();
		const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		TArray<FFrameNumber> Frames;
		const FFrameRate& FrameRate = Sequencer->GetFocusedDisplayRate();
		FFrameNumber FrameRateInFrameNumber = TickResolution.AsFrameNumber(FrameRate.AsInterval());
		if (TimeUnit == ESequenceTimeUnit::DisplayRate)
		{
			InSettings.StartFrame = FFrameRate::TransformTime(FFrameTime(InSettings.StartFrame, 0), FrameRate, TickResolution).RoundToFrame();
			InSettings.EndFrame = FFrameRate::TransformTime(FFrameTime(InSettings.EndFrame, 0), FrameRate, TickResolution).RoundToFrame();
		}
		for (FFrameNumber& Frame = InSettings.StartFrame; Frame <= InSettings.EndFrame; Frame += FrameRateInFrameNumber)
		{
			Frames.Add(Frame);
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
						Frames, InControlRig->GetHierarchy(), Element->GetKey(), InSettings);
				}
			}
		}
		bValid = true;
	}
	return bValid;
}

bool UControlRigSequencerEditorLibrary::DeleteControlRigSpace(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber InTime, ESequenceTimeUnit TimeUnit)
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

				if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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

bool UControlRigSequencerEditorLibrary::MoveControlRigSpace(ULevelSequence* LevelSequence, UControlRig* ControlRig, FName ControlName, FFrameNumber InTime, FFrameNumber InNewTime, ESequenceTimeUnit TimeUnit)
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

				if (TimeUnit == ESequenceTimeUnit::DisplayRate)
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

bool UControlRigSequencerEditorLibrary::GetControlsMask(UMovieSceneSection* InSection, FName ControlName)
{
	UMovieSceneControlRigParameterSection* ParameterSection = Cast<UMovieSceneControlRigParameterSection>(InSection);
	if (!ParameterSection)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot call GetControlsMask without a UMovieSceneControlRigParameterSection"), ELogVerbosity::Error);
		return false;
	}
	
	UControlRig* ControlRig = ParameterSection->GetControlRig();
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
		if (RigControl->GetName() == ControlName)
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

	UControlRig* ControlRig = ParameterSection->GetControlRig();
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
		if (ControlNames.Contains(RigControl->GetName()))
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

#undef LOCTEXT_NAMESPACE

