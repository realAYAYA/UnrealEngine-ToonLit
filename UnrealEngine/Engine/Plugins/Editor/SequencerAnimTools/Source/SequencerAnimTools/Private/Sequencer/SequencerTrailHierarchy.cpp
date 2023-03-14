// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerTrailHierarchy.h"

#include "MovieSceneSequence.h"
#include "ISequencer.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneSection.h"

#include "ControlRig.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "AnimationBoneTrail.h"
#include "LevelSequencePlayer.h"
#include "Tools/MotionTrailOptions.h"
#include "MovieSceneTransformTrail.h"
#include "IControlRigObjectBinding.h"
#include "ActorForWorldTransforms.h"
#include "MovieSceneToolHelpers.h"
#include "ControlRigSequencerEditorLibrary.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"

namespace UE
{
namespace SequencerAnimTools
{


void FSequencerTrailHierarchy::Initialize()
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer)
	{
		return;
	}
	UpdateViewAndEvalRange();

	TArray<FGuid> SequencerSelectedObjects;
	Sequencer->GetSelectedObjects(SequencerSelectedObjects);
	UpdateSequencerBindings(SequencerSelectedObjects,
		[this](UObject* Object, FTrail*, FGuid Guid) {
		VisibilityManager.Selected.Add(Guid);
	});
	
	OnSelectionChangedHandle = Sequencer->GetSelectionChangedObjectGuids().AddLambda([this](TArray<FGuid> NewSelection)
	{
	
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		check(Sequencer);
		TSet<FGuid> OldSelected = VisibilityManager.Selected;
		TSet<FGuid> NewSelected;
		
		auto SetVisibleFunc = [this, &NewSelected](UObject* Object, FTrail* TrailPtr, FGuid Guid) {
			NewSelected.Add(Guid);
		};

		UpdateSequencerBindings(NewSelection, SetVisibleFunc);
		for (FGuid Guid : OldSelected)
		{
			if (NewSelected.Find(Guid) == nullptr)
			{
				RemoveTrailIfNotAlwaysVisible(Guid);
			}
		}
		VisibilityManager.Selected = NewSelected;

	});

	OnViewOptionsChangedHandle = UMotionTrailToolOptions::GetTrailOptions()->OnDisplayPropertyChanged.AddLambda([this](FName PropertyName) {
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMotionTrailToolOptions, EvalsPerFrame))
		{
			for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
			{
				GuidTrailPair.Value->ForceEvaluateNextTick();
			}
		}
	});


	GEngine->OnLevelActorAdded().AddRaw(this, &FSequencerTrailHierarchy::OnActorChangedSomehow);
	GEngine->OnLevelActorDeleted().AddRaw(this, &FSequencerTrailHierarchy::OnActorChangedSomehow);
	GEngine->OnActorMoved().AddRaw(this, &FSequencerTrailHierarchy::OnActorChangedSomehow);
	GEngine->OnActorsMoved().AddRaw(this, &FSequencerTrailHierarchy::OnActorsChangedSomehow);
	//GEditor->RegisterForUndo(this);

}

void FSequencerTrailHierarchy::Destroy()
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer)
	{
		Sequencer->OnActorAddedToSequencer().Remove(OnActorAddedToSequencerHandle);
		Sequencer->GetSelectionChangedObjectGuids().Remove(OnSelectionChangedHandle);
		UMotionTrailToolOptions::GetTrailOptions()->OnDisplayPropertyChanged.Remove(OnViewOptionsChangedHandle);
	}

	for (const TPair<UMovieSceneControlRigParameterTrack*, FControlRigDelegateHandles>& SectionHandlesPair : ControlRigDelegateHandles)
	{
		UMovieSceneControlRigParameterTrack* Track = (SectionHandlesPair.Key);
		if (Track && Track->GetControlRig())
		{
			URigHierarchy* RigHierarchy = Track->GetControlRig()->GetHierarchy();
			Track->GetControlRig()->ControlSelected().Remove(SectionHandlesPair.Value.OnControlSelected);
			RigHierarchy->OnModified().Remove(SectionHandlesPair.Value.OnHierarchyModified);
		}
	}

	ObjectsTracked.Reset();
	BonesTracked.Reset();
	ControlsTracked.Reset();
	AllTrails.Reset();

	GEngine->OnLevelActorAdded().RemoveAll(this);
	GEngine->OnLevelActorDeleted().RemoveAll(this);
	GEngine->OnActorMoved().RemoveAll(this);
	GEngine->OnActorsMoved().RemoveAll(this);
	//GEditor->UnregisterForUndo(this);

}

void FSequencerTrailHierarchy::OnActorChangedSomehow(AActor* InActor)
{
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		USceneComponent* Component = Cast<USceneComponent>(GuidTrailPair.Value->GetOwner());
		if (Component)
		{
			TArray<const UObject*> ParentActors;
			UObject* Parent = Component->GetOwner();
			MovieSceneToolHelpers::GetParents(ParentActors, Parent);
			if (ParentActors.Contains(InActor))
			{
				GuidTrailPair.Value->ForceEvaluateNextTick();
			}
		}
	}
}

void FSequencerTrailHierarchy::OnActorsChangedSomehow(TArray<AActor*>& InActors)
{
	for (const TPair<FGuid, TUniquePtr<FTrail>>& GuidTrailPair : GetAllTrails())
	{
		for (AActor* InActor : InActors)
		{
			USceneComponent* Component = Cast<USceneComponent>(GuidTrailPair.Value->GetOwner());
			if (Component)
			{
				TArray<const UObject*> ParentActors;
				UObject* Parent = Component->GetOwner();
				MovieSceneToolHelpers::GetParents(ParentActors, Parent);
				if (ParentActors.Contains(InActor))
				{
					GuidTrailPair.Value->ForceEvaluateNextTick();
				}
			}
		}
	}
}

double FSequencerTrailHierarchy::GetSecondsPerSegment() const 
{ 
	double SecondsPerFrame =  WeakSequencer.Pin()->GetFocusedDisplayRate().AsInterval();
	//evalsperframe will not be less than one
	return SecondsPerFrame / double(UMotionTrailToolOptions::GetTrailOptions()->EvalsPerFrame);
}

FFrameNumber FSequencerTrailHierarchy::GetFramesPerSegment() const
{
	FFrameNumber FramesPerTick = FFrameRate::TransformTime(FFrameNumber(1), WeakSequencer.Pin()->GetFocusedDisplayRate(), 
		WeakSequencer.Pin()->GetFocusedTickResolution()).RoundToFrame();
	FramesPerTick.Value /= (UMotionTrailToolOptions::GetTrailOptions()->EvalsPerFrame);
	return FramesPerTick;
}

FFrameNumber FSequencerTrailHierarchy::GetFramesPerFrame() const
{
	FFrameNumber FramesPerTick = FFrameRate::TransformTime(FFrameNumber(1), WeakSequencer.Pin()->GetFocusedDisplayRate(),
		WeakSequencer.Pin()->GetFocusedTickResolution()).RoundToFrame();

	return FramesPerTick;
}

void FSequencerTrailHierarchy::RemoveTrail(const FGuid& Key)
{
	FTrailHierarchy::RemoveTrail(Key);
	if (UObject* const* FoundObject = ObjectsTracked.FindKey(Key))
	{
		ObjectsTracked.Remove(*FoundObject);
	}
	else
	{
		for (TPair<USkeletalMeshComponent*, TMap<FName, FGuid>>& CompMapPair : BonesTracked)
		{
			if (const FName* FoundBone = CompMapPair.Value.FindKey(Key))
			{
				CompMapPair.Value.Remove(*FoundBone);
				return;
			}
		}
		for (TPair<UControlRig*, TMap<FName, FGuid>>& CompMapPair : ControlsTracked)
		{
			if (const FName* FoundControl = CompMapPair.Value.FindKey(Key))
			{
				CompMapPair.Value.Remove(*FoundControl);
				return;
			}
		}
	}
}
struct FTrailControlTransforms
{
	FName ControlName;
	FTrail* Trail;
	TArray<FTransform> Transforms;
};

void FSequencerTrailHierarchy::UpdateControlRig(const TArray<FFrameNumber> &Frames,UControlRig* ControlRig, TMap<FName, FGuid >& CompMapPair, bool bUseEditedTimes)
{
	if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

		UWorld* World = GEditor->GetEditorWorldContext().World();
		ALevelSequenceActor* OutActor = nullptr;
		FMovieSceneSequencePlaybackSettings Settings;
		FLevelSequenceCameraSettings CameraSettings;
		FMovieSceneSequenceIDRef Template = MovieSceneSequenceID::Root;
		FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();
		IMovieScenePlayer* Player = Sequencer.Get();
		ULevelSequence* LevelSequence = Cast<ULevelSequence>(Sequencer->GetFocusedMovieSceneSequence());

		USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
		if (Component && Player)
		{
			AActor* Actor = Component->GetTypedOuter< AActor >();
			if (Actor)
			{
				TArray<FTransform> ControlRigParentWorldTransforms;
				FActorForWorldTransforms ControlRigActorSelection;
				ControlRigActorSelection.Actor = Actor;

				MovieSceneToolHelpers::GetActorWorldTransforms(Player, LevelSequence, Template, ControlRigActorSelection, Frames, ControlRigParentWorldTransforms);

				TArray<FTrailControlTransforms> TrailControlTransforms;
				TrailControlTransforms.SetNum(CompMapPair.Num());
				int32 PairIndex = 0;
				for (TPair<FName, FGuid >& Pair : CompMapPair)
				{
					TrailControlTransforms[PairIndex].Trail = AllTrails[Pair.Value].Get();
					TrailControlTransforms[PairIndex].ControlName = Pair.Key;
					TrailControlTransforms[PairIndex].Transforms.SetNum(Frames.Num());
					++PairIndex;
				}

				for (int32 Index = 0; Index < Frames.Num(); ++Index)
				{
					const FFrameNumber& FrameNumber = Frames[Index];
					FFrameTime GlobalTime(FrameNumber);
					GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly(); //player evals in root time so need to go back to it.

					FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);

					Player->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *Player);
					ControlRig->Evaluate_AnyThread();
					for (FTrailControlTransforms& TrailControlTransform : TrailControlTransforms)
					{
						TrailControlTransform.Transforms[Index] = ControlRig->GetControlGlobalTransform(TrailControlTransform.ControlName) * ControlRigParentWorldTransforms[Index];
						if (bUseEditedTimes)
						{
							double Sec = TickResolution.AsSeconds(FFrameTime(FrameNumber));
							TrailControlTransform.Trail->GetTrajectoryTransforms()->Set(Sec, TrailControlTransform.Transforms[Index]);
						}
					}
				}
				for (FTrailControlTransforms& TrailControlTransform : TrailControlTransforms)
				{
					if (bUseEditedTimes == false)
					{
						TrailControlTransform.Trail->GetTrajectoryTransforms()->SetTransforms(TrailControlTransform.Transforms, ControlRigParentWorldTransforms);
					}
					TrailControlTransform.Trail->UpdateKeysInRange(GetViewRange());
				}
			}
		}
		if (Player)
		{
			FFrameTime StartTime = Sequencer->GetLocalTime().Time;
			StartTime = StartTime * RootToLocalTransform.InverseLinearOnly(); //player evals in root time so need to go back to it.
			FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(StartTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);
			Player->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *Player);
			ControlRig->Evaluate_AnyThread();
		}
	}
}

void FSequencerTrailHierarchy::UpdateControlRig(const FTrailEvaluateTimes& EvaluateTimes,UControlRig* ControlRig, TMap<FName, FGuid > &CompMapPair)
{
	if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		check(Sequencer);
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		TArray<FFrameNumber> Frames;
		bool bUseEditedTimes = false;

		const FFrameTime LastFrame = EvaluateTimes.EvalTimes[EvaluateTimes.EvalTimes.Num() - 1] * TickResolution;
		for (TPair<FName, FGuid >& Pair : CompMapPair)
		{
			if (AllTrails[Pair.Value]->GetEditedTimes(this,LastFrame.RoundToFrame(), Frames))
			{
				bUseEditedTimes = true;
				break;
			}
		}
		int32 Index = 0;
		if (bUseEditedTimes == false)
		{
			//Todo mz make time in frames
			Frames.SetNum(EvaluateTimes.EvalTimes.Num());
			for (const double Time : EvaluateTimes.EvalTimes)
			{
				const FFrameTime TickTime = Time * TickResolution;
				Frames[Index++] = TickTime.RoundToFrame();
			}
		}
		UpdateControlRig(Frames, ControlRig, CompMapPair, bUseEditedTimes);
	}
}
void FSequencerTrailHierarchy::Update()
{
	const FDateTime UpdateStartTime = FDateTime::Now();

	UpdateViewAndEvalRange();
	FTrailHierarchy::Update();  //calculates EvalTimesArr
	FTrailEvaluateTimes EvalTimes = FTrailEvaluateTimes(EvalTimesArr, SecondsPerSegment);

	for (TPair<UControlRig*, TMap<FName, FGuid>>& CompMapPair : ControlsTracked)
	{
		bool bNeedToUpdateControlRig = false;
		FTrail* ForceTrail = nullptr;
		for (TPair<FName, FGuid>& Pair : CompMapPair.Value)
		{
			if (AllTrails[Pair.Value]->GetCacheState() == ETrailCacheState::Stale)
			{
				bNeedToUpdateControlRig = true;
				ForceTrail = AllTrails[Pair.Value].Get();
				break;
			}
		}
		if (bNeedToUpdateControlRig == true)
		{
			FMovieSceneControlRigTransformTrail* ControlRigTrail = static_cast<FMovieSceneControlRigTransformTrail*>(ForceTrail);
				

			//if the mouse is not down we update everything like normal making sure we don't use keys to draw the trajectory
			//if mouse is down we make sure to just use keys to draw the trajectory and we figure out what times we just need to update
			//which will be selected keys and the current time.
			if (FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton) == false)
			{
				if (ControlRigTrail)
				{
					ControlRigTrail->SetUseKeysForTrajectory(false);
				}
				UpdateControlRig(EvalTimes, CompMapPair.Key, CompMapPair.Value);
			}
			else if(ForceTrail)
			{
				if (ControlRigTrail)
				{
					ControlRigTrail->SetUseKeysForTrajectory(true);
				}
				//just do selected key times and playback time
				TArray<FFrameNumber> Frames = ForceTrail->GetSelectedKeyTimes();
				TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
				FFrameNumber CurrentFrame = SequencerPtr->GetLocalTime().Time.GetFrame();
				if (Frames.Contains(CurrentFrame) == false)
				{
					Frames.Add(CurrentFrame);
				}
				UpdateControlRig(Frames, CompMapPair.Key, CompMapPair.Value, true);
				ForceTrail->ForceEvaluateNextTick();
			}
		}
	}

	const FTimespan UpdateTimespan = FDateTime::Now() - UpdateStartTime;
	TimingStats.Add("FSequencerTrailHierarchy::Update", UpdateTimespan);
}

void FSequencerTrailHierarchy::OnBoneVisibilityChanged(class USkeleton* Skeleton, const FName& BoneName, const bool bIsVisible)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(Sequencer);

	TArray<FGuid> SelectedSequencerGuids;
	Sequencer->GetSelectedObjects(SelectedSequencerGuids);

	// TODO: potentially expensive
	for (const FGuid& SelectedGuid : SelectedSequencerGuids)
	{
		for (TWeakObjectPtr<> BoundObject : Sequencer->FindObjectsInCurrentSequence(SelectedGuid))
		{
			USkeletalMeshComponent* BoundComponent = Cast<USkeletalMeshComponent>(BoundObject.Get());
			if (AActor* BoundActor = Cast<AActor>(BoundObject.Get()))
			{
				BoundComponent = BoundActor->FindComponentByClass<USkeletalMeshComponent>();
			}

			if (!BoundComponent || !BoundComponent->GetSkeletalMeshAsset() || !BoundComponent->GetSkeletalMeshAsset()->GetSkeleton() || !(BoundComponent->GetSkeletalMeshAsset()->GetSkeleton() == Skeleton) || !BonesTracked.Contains(BoundComponent))
			{
				continue;
			}

			const FGuid BoneTrailGuid = BonesTracked[BoundComponent][BoneName];
			const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
			
			if (bIsVisible)
			{
				VisibilityManager.VisibilityMask.Remove(BoneTrailGuid);
				VisibilityManager.Selected.Add(BoneTrailGuid);
			}
			else
			{
				VisibilityManager.VisibilityMask.Add(BoneTrailGuid);
				VisibilityManager.Selected.Remove(BoneTrailGuid);
			}
		}
	}
}

void FSequencerTrailHierarchy::OnBindingVisibilityStateChanged(UObject* BoundObject, const EBindingVisibilityState VisibilityState)
{
	auto UpdateTrailVisibilityState = [this, VisibilityState](const FGuid& Guid) {
		if (VisibilityState == EBindingVisibilityState::AlwaysVisible)
		{
			VisibilityManager.AlwaysVisible.Add(Guid);
		}
		else if (VisibilityState == EBindingVisibilityState::VisibleWhenSelected)
		{
			VisibilityManager.AlwaysVisible.Remove(Guid);
		}
	};

	if (ObjectsTracked.Contains(BoundObject))
	{
		UpdateTrailVisibilityState(ObjectsTracked[BoundObject]);
	}

	USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(BoundObject);
	if (!SkelMeshComp)
	{
		return;
	}

	for (const TPair<FName, FGuid>& Pair : BonesTracked.FindRef(SkelMeshComp))
	{
		UpdateTrailVisibilityState(Pair.Value);
	}
	/* to do mz
	for (const TPair<FName, FGuid>& Pair : ControlsTracked.FindRef(SkelMeshComp))
	{
		UpdateTrailVisibilityState(ControlsTracked[SkelMeshComp][Pair.Key]);
	}
	*/
}

void FSequencerTrailHierarchy::UpdateSequencerBindings(const TArray<FGuid>& SequencerBindings, TFunctionRef<void(UObject*, FTrail*, FGuid)> OnUpdated)
{
	const FDateTime StartTime = FDateTime::Now();

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(Sequencer);

	for (FGuid BindingGuid : SequencerBindings)
	{
		if (UMovieScene3DTransformTrack* TransformTrack = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(BindingGuid))
		{
			for (TWeakObjectPtr<> BoundObject : Sequencer->FindBoundObjects(BindingGuid, Sequencer->GetFocusedTemplateID()))
			{
				if (!BoundObject.IsValid())
				{
					continue;
				}
				//if using old trails don't add new ones.
				if (CVarUseOldSequencerMotionTrails->GetBool() == true)
				{
					continue;
				}

				USceneComponent* BoundComponent = Cast<USceneComponent>(BoundObject.Get());
				if (AActor* BoundActor = Cast<AActor>(BoundObject.Get()))
				{
					BoundComponent = BoundActor->GetRootComponent();
				}

				if (!ObjectsTracked.Contains(BoundComponent))
				{
					AddComponentToHierarchy(BoundComponent, TransformTrack);
				}

				if (!ObjectsTracked.Contains(BoundComponent))
				{
					continue;
				}

				if (AllTrails.Contains(ObjectsTracked[BoundComponent]) && AllTrails[ObjectsTracked[BoundComponent]].IsValid())
				{
					OnUpdated(BoundComponent, AllTrails[ObjectsTracked[BoundComponent]].Get(), ObjectsTracked[BoundComponent]);
				}
			}
		} // if TransformTrack
		if (UMovieSceneSkeletalAnimationTrack* AnimTrack = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieSceneSkeletalAnimationTrack>(BindingGuid))
		{
			for (TWeakObjectPtr<> BoundObject : Sequencer->FindBoundObjects(BindingGuid, Sequencer->GetFocusedTemplateID()))
			{
				if (!BoundObject.IsValid())
				{
					continue;
				}
				if (CVarUseOldSequencerMotionTrails->GetBool() == true)
				{
					continue;
				}

				USkeletalMeshComponent* BoundComponent = Cast<USkeletalMeshComponent>(BoundObject.Get());
				if (AActor* BoundActor = Cast<AActor>(BoundObject.Get()))
				{
					BoundComponent = BoundActor->FindComponentByClass<USkeletalMeshComponent>();
				}

				if (!BoundComponent || !BoundComponent->GetSkeletalMeshAsset() || !BoundComponent->GetSkeletalMeshAsset()->GetSkeleton())
				{
					continue;
				}

				if (!ObjectsTracked.Contains(BoundComponent))
				{
					if (UMovieScene3DTransformTrack* TransformTrack = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(BindingGuid))
					{
						AddComponentToHierarchy(BoundComponent, TransformTrack);
					}
				}

				if (!ObjectsTracked.Contains(BoundComponent))
				{
					continue;
				}

				if (!BonesTracked.Contains(BoundComponent))
				{
					//AddSkeletonToHierarchy(BoundComponent); //need to revisit skeletals
				}

				if (!BonesTracked.Contains(BoundComponent))
				{
					continue;
				}

				for (const TPair<FName, FGuid>& BoneNameGuidPair : BonesTracked[BoundComponent])
				{
					if (AllTrails.Contains(BoneNameGuidPair.Value) && AllTrails[BoneNameGuidPair.Value].IsValid())
					{
						OnUpdated(BoundComponent, AllTrails[BoneNameGuidPair.Value].Get(), BoneNameGuidPair.Value);
					}
				}
			}
		}
		if (UMovieSceneControlRigParameterTrack* CRParameterTrack = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieSceneControlRigParameterTrack>(BindingGuid))
		{
			for (TWeakObjectPtr<> BoundObject : Sequencer->FindBoundObjects(BindingGuid, Sequencer->GetFocusedTemplateID()))
			{
				if (!BoundObject.IsValid())
				{
					continue;
				}

				USkeletalMeshComponent* BoundComponent = Cast<USkeletalMeshComponent>(BoundObject.Get());
				if (AActor* BoundActor = Cast<AActor>(BoundObject.Get()))
				{
					BoundComponent = BoundActor->FindComponentByClass<USkeletalMeshComponent>();
				}

				if (!BoundComponent || !BoundComponent->GetSkeletalMeshAsset() || !BoundComponent->GetSkeletalMeshAsset()->GetSkeleton())
				{
					continue;
				};

				if (!ControlRigDelegateHandles.Contains(CRParameterTrack))
				{
					RegisterControlRigDelegates(BoundComponent, CRParameterTrack);
					UControlRig* ControlRig = CRParameterTrack->GetControlRig();
					if (ControlRig)
					{
						TArray<FName> Selected = ControlRig->CurrentControlSelection();
						for (const FName& ControlName : Selected)
						{
							AddControlRigTrail(BoundComponent,ControlRig, CRParameterTrack, ControlName);
						}
					}
				}
			}
		} // if ControlRigParameterTrack

	}
	const FTimespan Timespan = FDateTime::Now() - StartTime;
	TimingStats.Add("FSequencerTrailHierarchy::UpdateSequencerBindings", Timespan);
}


void FSequencerTrailHierarchy::UpdateViewAndEvalRange()
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(Sequencer);

	FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
	FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();

	TRange<FFrameNumber> TickViewRange;
	TRange<FFrameNumber> EvalViewRange  = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
	if (!UMotionTrailToolOptions::GetTrailOptions()->bShowFullTrail)
	{
		FFrameTime SequenceTime = Sequencer->GetLocalTime().Time;
		const FFrameNumber TicksBefore = FFrameRate::TransformTime(FFrameNumber(UMotionTrailToolOptions::GetTrailOptions()->FramesBefore), DisplayRate, TickResolution).FloorToFrame();
		const FFrameNumber TicksAfter = FFrameRate::TransformTime(FFrameNumber(UMotionTrailToolOptions::GetTrailOptions()->FramesAfter), DisplayRate, TickResolution).FloorToFrame();
		TickViewRange = TRange<FFrameNumber>(SequenceTime.GetFrame() - TicksBefore, SequenceTime.GetFrame() + TicksAfter);
	}
	else
	{
		TickViewRange = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
	}

	const double StartViewSeconds = TickResolution.AsSeconds(FFrameTime(TickViewRange.GetLowerBoundValue()));
	const double EndViewSeconds = TickResolution.AsSeconds(FFrameTime(TickViewRange.GetUpperBoundValue()));
	ViewRange = TRange<double>(StartViewSeconds,EndViewSeconds);

	const double StartEvalSeconds = TickResolution.AsSeconds(FFrameTime(EvalViewRange.GetLowerBoundValue()));
	const double EndEvalSeconds = TickResolution.AsSeconds(FFrameTime(EvalViewRange.GetUpperBoundValue()));
	EvalRange = TRange<double>(StartEvalSeconds, EndEvalSeconds);

	// snap view range to ticks per segment
	//const double TicksBetween = FMath::Fmod(StartSeconds, GetSecondsPerSegment());
	//ViewRange = TRange<double>(StartSeconds - TicksBetween, EndSeconds - TicksBetween);

}

void FSequencerTrailHierarchy::AddComponentToHierarchy(USceneComponent* CompToAdd, UMovieScene3DTransformTrack* TransformTrack)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(Sequencer);
	
	const FGuid CurTrailGuid = ObjectsTracked.FindOrAdd(CompToAdd, FGuid::NewGuid());

	TUniquePtr<FTrail> CurTrail = MakeUnique<FMovieSceneComponentTransformTrail>(CompToAdd,UMotionTrailToolOptions::GetTrailOptions()->TrailColor, false, TransformTrack, Sequencer);
	if (AllTrails.Contains(ObjectsTracked[CompToAdd])) 
	{
		AllTrails.Remove(ObjectsTracked[CompToAdd]);
	}
	CurTrail->ForceEvaluateNextTick();

	AddTrail(ObjectsTracked[CompToAdd], MoveTemp(CurTrail));
}

void FSequencerTrailHierarchy::AddSkeletonToHierarchy(class USkeletalMeshComponent* CompToAdd)
{
	const FDateTime StartTime = FDateTime::Now();

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(Sequencer);

	TSharedPtr<FAnimTrajectoryCache> AnimTrajectoryCache = MakeShared<FAnimTrajectoryCache>(CompToAdd, Sequencer);
	TMap<FName, FGuid>& BoneMap = BonesTracked.Add(CompToAdd, TMap<FName, FGuid>());
	
	USkeleton* MySkeleton = CompToAdd->GetSkeletalMeshAsset()->GetSkeleton();
	const int32 NumBones = MySkeleton->GetReferenceSkeleton().GetNum();
	for (int32 BoneIdx = 0; BoneIdx < NumBones; BoneIdx++)
	{
		int32 ParentBoneIndex = MySkeleton->GetReferenceSkeleton().GetParentIndex(BoneIdx);
		const FName BoneName = MySkeleton->GetReferenceSkeleton().GetBoneName(BoneIdx);

		const FGuid BoneGuid = FGuid::NewGuid();
		BoneMap.Add(BoneName, BoneGuid);
		VisibilityManager.VisibilityMask.Add(BoneGuid);

		if (ParentBoneIndex != INDEX_NONE)
		{
			AllTrails.Add(BoneGuid, MakeUnique<FAnimationBoneTrail>(CompToAdd,UMotionTrailToolOptions::GetTrailOptions()->TrailColor, false, AnimTrajectoryCache, BoneName, false));

		}
		else // root bone
		{
			AllTrails.Add(BoneGuid, MakeUnique<FAnimationBoneTrail>(CompToAdd,UMotionTrailToolOptions::GetTrailOptions()->TrailColor, false, AnimTrajectoryCache, BoneName, true));
		}
	}

	const FTimespan Timespan = FDateTime::Now() - StartTime;
	TimingStats.Add("FSequencerTrailHierarchy::AddSkeletonToHierarchy", Timespan);
}

void FSequencerTrailHierarchy::AddControlRigTrail(USkeletalMeshComponent* Component, UControlRig* ControlRig, UMovieSceneControlRigParameterTrack* CRParameterTrack,const FName& ControlName)
{
	if (ControlsTracked.Find(ControlRig) == nullptr)
	{
		ControlsTracked.Add(ControlRig, TMap<FName, FGuid>());
	}
	if (!ControlsTracked[ControlRig].Contains(ControlName))
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer)
		{
			const FGuid ControlGuid = ControlsTracked.Find(ControlRig)->FindOrAdd(ControlName, FGuid::NewGuid());

			TUniquePtr<FTrail> CurTrail = MakeUnique<FMovieSceneControlRigTransformTrail>(Component, UMotionTrailToolOptions::GetTrailOptions()->TrailColor, false, CRParameterTrack, Sequencer, ControlName);
			if (AllTrails.Contains(ControlGuid))
			{
				AllTrails.Remove(ControlGuid);
				VisibilityManager.ControlSelected.Remove(ControlGuid);
			}
			AddTrail(ControlGuid, MoveTemp(CurTrail));
			VisibilityManager.ControlSelected.Add(ControlGuid);
		}
	}
	else
	{
		const FGuid* ControlGuid = ControlsTracked.Find(ControlRig)->Find(ControlName);
		if (ControlGuid != nullptr)
		{
			VisibilityManager.ControlSelected.Add(*ControlGuid);
		}
	}
}

void FSequencerTrailHierarchy::RegisterControlRigDelegates(USkeletalMeshComponent* Component, UMovieSceneControlRigParameterTrack* CRParameterTrack)
{
	UControlRig* ControlRig = CRParameterTrack->GetControlRig();
	URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
	FControlRigDelegateHandles& DelegateHandles = ControlRigDelegateHandles.Add(CRParameterTrack);
	DelegateHandles.OnControlSelected = ControlRig->ControlSelected().AddLambda([this, Component,CRParameterTrack](UControlRig* ControlRig, FRigControlElement* ControlElement, bool bSelected)
		{
			if (ControlElement->Settings.ControlType != ERigControlType::Transform &&
				ControlElement->Settings.ControlType != ERigControlType::TransformNoScale &&
				ControlElement->Settings.ControlType != ERigControlType::EulerTransform)
			{
				return;
			}

			if (bSelected)
			{
				AddControlRigTrail(Component,ControlRig,CRParameterTrack, ControlElement->GetName());
			}

			if (ControlsTracked.Find(ControlRig) != nullptr)
			{
				if (ControlsTracked[ControlRig].Contains(ControlElement->GetName()))
				{
					if (bSelected == false)
					{
						const FGuid TrailGuid = ControlsTracked[ControlRig][ControlElement->GetName()];
						VisibilityManager.ControlSelected.Remove(TrailGuid);
						RemoveTrailIfNotAlwaysVisible(TrailGuid);
					}
				}
			}

			//check to see if the seleced control rig is sill selected
			TArray<FGuid> TrailsToRemove;
			for (TPair<UControlRig*, TMap<FName, FGuid>>& CompMapPair : ControlsTracked)
			{
				UControlRig* TrackedControlRig = CompMapPair.Key;
				for (TPair<FName, FGuid> NameGuid : CompMapPair.Value)
				{
					if (TrackedControlRig->IsControlSelected(NameGuid.Key) == false)
					{
						FGuid TrailGuid = NameGuid.Value;
						TrailsToRemove.Add(TrailGuid);
					}
				}
			}
			for (const FGuid TrailGuid: TrailsToRemove)
			{
				VisibilityManager.ControlSelected.Remove(TrailGuid);
				RemoveTrailIfNotAlwaysVisible(TrailGuid);
			}
		}

			
	);
	
	DelegateHandles.OnHierarchyModified = RigHierarchy->OnModified().AddLambda(
		[this, RigHierarchy, ControlRig](ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement) {
			TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
			check(Sequencer);

			const FRigControlElement* ControlElement = Cast<FRigControlElement>(InElement); 
			if(ControlElement == nullptr)
			{
				return;
			}

			
			if(InNotif == ERigHierarchyNotification::ElementRemoved)
			{
				if (!ControlsTracked.Contains(ControlRig) || !ControlsTracked[ControlRig].Contains(ControlElement->GetName())) // We only care about controls
				{
					return;
				}
				
				const FGuid TrailGuid = ControlsTracked[ControlRig][ControlElement->GetName()];
				RemoveTrail(TrailGuid);
			}
            else if(InNotif == ERigHierarchyNotification::ElementRenamed)
            {
            	const FName OldName = InHierarchy->GetPreviousName(ControlElement->GetKey());

            	if (!ControlsTracked.Contains(ControlRig) || !ControlsTracked[ControlRig].Contains(OldName))
				{
					return;
				}

				const FGuid TempTrailGuid = ControlsTracked[ControlRig][OldName];
				ControlsTracked[ControlRig].Remove(OldName);
				ControlsTracked[ControlRig].Add(ControlElement->GetName(), TempTrailGuid);
            }
        }
    );
}

} // namespace MovieScene
} // namespace UE
