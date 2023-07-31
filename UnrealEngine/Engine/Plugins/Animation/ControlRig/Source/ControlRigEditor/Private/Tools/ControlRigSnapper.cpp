// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ControlRigSnapper.h"
#include "Tools/ControlRigTweener.h" //remove

#include "Channels/MovieSceneFloatChannel.h"
#include "ControlRig.h"
#include "ISequencer.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Rigs/RigControlHierarchy.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "IKeyArea.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "ILevelSequenceEditorToolkit.h"
#include "Tools/ControlRigSnapper.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"
#include "ControlRigObjectBinding.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Tools/ControlRigSnapSettings.h"
#include "MovieSceneToolsModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "ScopedTransaction.h"
#include "MovieSceneToolHelpers.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "Tools/BakingHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigSnapper)

#define LOCTEXT_NAMESPACE "ControlRigSnapper"


FText FControlRigSnapperSelection::GetName() const
{
	FText Name;

	int32 Num = NumSelected();
	if (Num == 0)
	{
		Name = LOCTEXT("None", "None");
	}
	else if (Num == 1)
	{
		for (const FActorForWorldTransforms& Actor : Actors)
		{
			if (Actor.Actor.IsValid())
			{
				FString ActorLabel = Actor.Actor->GetActorLabel();
				if (Actor.SocketName != NAME_None)
				{
					FString SocketString = Actor.SocketName.ToString();
					ActorLabel += (FString(":") + SocketString);
				}
				Name = FText::FromString(ActorLabel);
				break;
			}
		}
		for (const FControlRigForWorldTransforms& Selection : ControlRigs)
		{
			if (Selection.ControlRig.IsValid())
			{
				if (Selection.ControlNames.Num() > 0)
				{
					FName ControlName = Selection.ControlNames[0];
					Name = FText::FromString(ControlName.ToString());
					break;
				}
			}
		}
	}
	else
	{
		Name = LOCTEXT("Multiple", "--Multiple--");
	}
	return Name;
}

int32 FControlRigSnapperSelection::NumSelected() const
{
	int32 Selected = 0;
	for (const FActorForWorldTransforms& Actor : Actors)
	{
		if (Actor.Actor.IsValid())
		{
			++Selected;
		}
	}
	for (const FControlRigForWorldTransforms& Selection : ControlRigs)
	{
		if (Selection.ControlRig.IsValid())
		{
			Selected += Selection.ControlNames.Num();
		}
	}
	return Selected;
}

TWeakPtr<ISequencer> FControlRigSnapper::GetSequencer()
{
	return FBakingHelper::GetSequencer();
}

static bool LocalGetControlRigControlTransforms(IMovieScenePlayer* Player, const TOptional<FFrameNumber>& CurrentFrame, UMovieSceneSequence* MovieSceneSequence, FMovieSceneSequenceIDRef Template, FMovieSceneSequenceTransform& RootToLocalTransform,
	UControlRig* ControlRig, const FName& ControlName,
	const TArray<FFrameNumber>& Frames, const TArray<FTransform>& ParentTransforms, TArray<FTransform>& OutTransforms)
{
	if (Frames.Num() > ParentTransforms.Num())
	{
		UE_LOG(LogControlRig, Error, TEXT("Number of Frames %d to Snap greater  than Parent Frames %d"), Frames.Num(), ParentTransforms.Num());
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
		FFrameRate DisplayRate = MovieScene->GetDisplayRate();

		OutTransforms.SetNum(Frames.Num());
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			const FFrameNumber& FrameNumber = Frames[Index];
			if (CurrentFrame.IsSet() == false || CurrentFrame.GetValue() != FrameNumber)
			{
				FFrameTime GlobalTime(FrameNumber);
				GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly(); //player evals in root time so need to go back to it.

				FMovieSceneContext Context = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);
				Player->GetEvaluationTemplate().EvaluateSynchronousBlocking(Context, *Player);
			}
			ControlRig->Evaluate_AnyThread();
			OutTransforms[Index] = ControlRig->GetControlGlobalTransform(ControlName) * ParentTransforms[Index];
		}
	}
	return true;
}

bool FControlRigSnapper::GetControlRigControlTransforms(ISequencer* Sequencer,  UControlRig* ControlRig, const FName& ControlName,
	const TArray<FFrameNumber> &Frames, const TArray<FTransform>& ParentTransforms,TArray<FTransform>& OutTransforms)
{
	if (Sequencer->GetFocusedMovieSceneSequence())
	{
		FMovieSceneSequenceIDRef Template = Sequencer->GetFocusedTemplateID();
		FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();
		TOptional<FFrameNumber> FrameNumber = (Sequencer->GetLocalTime().Time * RootToLocalTransform.InverseLinearOnly()).RoundToFrame();
		return LocalGetControlRigControlTransforms(Sequencer, FrameNumber, Sequencer->GetFocusedMovieSceneSequence(), Template, RootToLocalTransform,
			ControlRig, ControlName, Frames, ParentTransforms, OutTransforms);
	
	}
	return false;
}

bool FControlRigSnapper::GetControlRigControlTransforms(UWorld* World,ULevelSequence* LevelSequence, UControlRig* ControlRig, const FName& ControlName,
	const TArray<FFrameNumber>& Frames, const TArray<FTransform>& ParentTransforms, TArray<FTransform>& OutTransforms)
{
	if (LevelSequence)
	{
		ALevelSequenceActor* OutActor;
		FMovieSceneSequencePlaybackSettings Settings;
		FLevelSequenceCameraSettings CameraSettings;
		FMovieSceneSequenceIDRef Template = MovieSceneSequenceID::Root;
		FMovieSceneSequenceTransform RootToLocalTransform;
		ULevelSequencePlayer* Player = ULevelSequencePlayer::CreateLevelSequencePlayer(World, LevelSequence, Settings, OutActor);
		TOptional<FFrameNumber> OptFrame;
		bool Success = LocalGetControlRigControlTransforms(Player, OptFrame, LevelSequence, Template, RootToLocalTransform,
			ControlRig, ControlName, Frames, ParentTransforms, OutTransforms);
		World->DestroyActor(OutActor);
		return Success;
	}
	return false;
}

struct FGuidAndActor
{
	FGuidAndActor(FGuid InGuid, AActor* InActor) : Guid(InGuid), Actor(InActor) {};
	FGuid Guid;
	AActor* Actor;

	bool SetTransform(
		ISequencer* Sequencer, const TArray<FFrameNumber>& Frames,
		const TArray<FTransform>& WorldTransformsToSnapTo, const UControlRigSnapSettings* SnapSettings) const
	{
		// get section
		const UMovieScene3DTransformSection* TransformSection = MovieSceneToolHelpers::GetTransformSection(Sequencer, Guid);
		if (!TransformSection)
		{
			return false;
		}

		// fill parent transforms
		TArray<FTransform> ParentWorldTransforms; 
		AActor* ParentActor = Actor->GetAttachParentActor();
		if (ParentActor)
		{
			FActorForWorldTransforms ActorSelection;
			ActorSelection.SocketName = Actor->GetAttachParentSocketName();
			MovieSceneToolHelpers::GetActorWorldTransforms(Sequencer, ActorSelection, Frames, ParentWorldTransforms);
		}
		else
		{
			ParentWorldTransforms.SetNumUninitialized(Frames.Num());
			for (FTransform& Transform : ParentWorldTransforms)
			{
				Transform = FTransform::Identity;
			}
		}

		// fill channels to key
		EMovieSceneTransformChannel Channels;
		if (SnapSettings->bSnapPosition)
		{
			EnumAddFlags(Channels, EMovieSceneTransformChannel::Translation);
		}
		if (SnapSettings->bSnapRotation)
		{
			EnumAddFlags(Channels, EMovieSceneTransformChannel::Rotation);
		}
		if (SnapSettings->bSnapScale)
		{
			EnumAddFlags(Channels, EMovieSceneTransformChannel::Scale);
		}

		// compute local transforms
		TArray<FTransform> LocalTransforms;
		LocalTransforms.Reserve(Frames.Num());
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			const FTransform& ParentTransform = ParentWorldTransforms[Index];
			const FTransform& WorldTransform = WorldTransformsToSnapTo[Index];
			LocalTransforms.Add(WorldTransform.GetRelativeTransform(ParentTransform));
		}

		// add keys
		MovieSceneToolHelpers::AddTransformKeys(TransformSection, Frames, LocalTransforms, Channels);

		// notify
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

		return true;
	}
};

//returns true if world is calculated, false if there are no parents
static bool CalculateWorldTransformsFromParents(ISequencer* Sequencer, const FControlRigSnapperSelection& ParentToSnap,
	const TArray<FFrameNumber>& Frames, TArray<FTransform>& OutParentWorldTransforms)
{
	//just do first for now but may do average later.
	for (const FActorForWorldTransforms& ActorSelection : ParentToSnap.Actors)
	{
		if (ActorSelection.Actor.IsValid())
		{
			MovieSceneToolHelpers::GetActorWorldTransforms(Sequencer, ActorSelection, Frames, OutParentWorldTransforms);
			return true;
		}
	}

	for (const FControlRigForWorldTransforms& ControlRigAndSelection : ParentToSnap.ControlRigs)
	{
		//get actor transform...
		UControlRig* ControlRig = ControlRigAndSelection.ControlRig.Get();

		if (ControlRig)
		{
			if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
			{
				USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
				if (!Component)
				{
					continue;
				}
				AActor* Actor = Component->GetTypedOuter< AActor >();
				if (!Actor)
				{
					continue;
				}
				TArray<FTransform> ParentTransforms;
				FActorForWorldTransforms ActorSelection;
				ActorSelection.Actor = Actor;
				MovieSceneToolHelpers::GetActorWorldTransforms(Sequencer, ActorSelection, Frames, ParentTransforms);

				//just do first for now but may do average later.
				FControlRigSnapper Snapper;
				for (const FName& Name : ControlRigAndSelection.ControlNames)
				{
					Snapper.GetControlRigControlTransforms(Sequencer, ControlRig, Name, Frames, ParentTransforms, OutParentWorldTransforms);
					return true;
				}
			}
		}
	}
	OutParentWorldTransforms.SetNum(Frames.Num());
	for (FTransform& Transform : OutParentWorldTransforms)
	{
		Transform = FTransform::Identity;
	}
	return false;
}


bool FControlRigSnapper::SnapIt(FFrameNumber StartFrame, FFrameNumber EndFrame,const FControlRigSnapperSelection& ActorToSnap,
	const FControlRigSnapperSelection& ParentToSnap, const UControlRigSnapSettings* SnapSettings)
{
	TWeakPtr<ISequencer> InSequencer = GetSequencer();
	if (InSequencer.IsValid() && InSequencer.Pin()->GetFocusedMovieSceneSequence() && ActorToSnap.IsValid())
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("SnapAnimation", "Snap Animation"));

		ISequencer* Sequencer = InSequencer.Pin().Get();
		Sequencer->ForceEvaluate(); // force an evaluate so any control rig get's binding setup
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		MovieScene->Modify();
		
		TArray<FFrameNumber> Frames;
		MovieSceneToolHelpers::CalculateFramesBetween(MovieScene, StartFrame, EndFrame, Frames);

		TArray<FTransform> WorldTransformToSnap;
		bool bSnapToFirstFrameNotParents = !CalculateWorldTransformsFromParents(Sequencer, ParentToSnap, Frames, WorldTransformToSnap);
		if (Frames.Num() != WorldTransformToSnap.Num())
		{
			UE_LOG(LogControlRig, Error, TEXT("Number of Frames %d to Snap different than Parent Frames %d"), Frames.Num(),WorldTransformToSnap.Num());
			return false;
		}

		//need to make sure binding is setup
		Sequencer->ForceEvaluate();
		
		TArray<FGuidAndActor > ActorsToSnap;
		//There may be Actors here not in Sequencer so we add them to sequencer also
		for (const FActorForWorldTransforms& ActorSelection : ActorToSnap.Actors)
		{
			if (ActorSelection.Actor.IsValid())
			{
				AActor* Actor = ActorSelection.Actor.Get();
				FGuid ObjectHandle = Sequencer->GetHandleToObject(Actor,false);
				if (ObjectHandle.IsValid() == false)
				{
					TArray<TWeakObjectPtr<AActor> > ActorsToAdd;
					ActorsToAdd.Add(Actor);
					TArray<FGuid> ActorTracks = Sequencer->AddActors(ActorsToAdd, false);
					if (ActorTracks[0].IsValid())
					{
						ActorsToSnap.Add(FGuidAndActor(ActorTracks[0], Actor));
					}
				}
				else
				{
					ActorsToSnap.Add(FGuidAndActor(ObjectHandle, Actor));
				}
			}
		}
		//set transforms on these actors
		for (FGuidAndActor& GuidActor : ActorsToSnap)
		{
			if (bSnapToFirstFrameNotParents || SnapSettings->bKeepOffset) //if we are snapping to the first frame or keep offset we just don't set the parent transforms
			{
				TArray<FFrameNumber> OneFrame;
				OneFrame.Add(Frames[0]);
				TArray<FTransform> OneTransform;
				FActorForWorldTransforms ActorSelection;
				ActorSelection.Actor = GuidActor.Actor;
				MovieSceneToolHelpers::GetActorWorldTransforms(Sequencer, ActorSelection, OneFrame, OneTransform);
				if (bSnapToFirstFrameNotParents)
				{
					for (FTransform& Transform : WorldTransformToSnap)
					{
						Transform = OneTransform[0];
					}
				}
				else //we keep offset
				{
					FTransform FirstWorld = WorldTransformToSnap[0];
					for (FTransform& Transform : WorldTransformToSnap)
					{
						Transform = OneTransform[0].GetRelativeTransform(FirstWorld) * Transform;
					}
				}

			}
			GuidActor.SetTransform(Sequencer, Frames, WorldTransformToSnap,SnapSettings);
		}

		//Now do Control Rigs
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;

		for (const FControlRigForWorldTransforms& ControlRigAndSelection : ActorToSnap.ControlRigs)
		{
			//get actor transform...
			UControlRig* ControlRig = ControlRigAndSelection.ControlRig.Get();

			if (ControlRig)
			{
				if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig->GetObjectBinding())
				{
					USceneComponent* Component = Cast<USceneComponent>(ObjectBinding->GetBoundObject());
					if (!Component)
					{
						continue;
					}
					AActor* Actor = Component->GetTypedOuter< AActor >();
					if (!Actor)
					{
						continue;
					}
					ControlRig->Modify();
					TArray<FTransform> ControlRigParentWorldTransforms;
					FActorForWorldTransforms ControlRigActorSelection;
					ControlRigActorSelection.Actor = Actor;
					MovieSceneToolHelpers::GetActorWorldTransforms(Sequencer, ControlRigActorSelection, Frames, ControlRigParentWorldTransforms);
					FControlRigSnapper Snapper;
					for (const FName& Name : ControlRigAndSelection.ControlNames)
					{
						TArray<FFrameNumber> OneFrame;
						OneFrame.SetNum(1);
						TArray<FTransform> CurrentControlRigTransform, CurrentParentWorldTransform;
						CurrentControlRigTransform.SetNum(1);
						CurrentParentWorldTransform.SetNum(1);
						if (bSnapToFirstFrameNotParents || SnapSettings->bKeepOffset)
						{
							OneFrame[0] = Frames[0];
							CurrentParentWorldTransform[0] = ControlRigParentWorldTransforms[0];
							Snapper.GetControlRigControlTransforms(Sequencer, ControlRig, Name, OneFrame, CurrentParentWorldTransform, CurrentControlRigTransform);
							if (bSnapToFirstFrameNotParents)
							{
								for (FTransform& Transform : WorldTransformToSnap)
								{
									Transform = CurrentControlRigTransform[0];
								}
							}
							else if (SnapSettings->bKeepOffset)
							{
								FTransform FirstWorld = WorldTransformToSnap[0];
								for (FTransform& Transform : WorldTransformToSnap)
								{
									Transform =  CurrentControlRigTransform[0].GetRelativeTransform(FirstWorld) * Transform;
								}
							}
						}

						for (int32 Index = 0; Index < WorldTransformToSnap.Num(); ++Index)
						{
							OneFrame[0] = Frames[Index];
							CurrentParentWorldTransform[0] = ControlRigParentWorldTransforms[Index];
							//this will evaluate at the current frame which we want
							GetControlRigControlTransforms(Sequencer, ControlRig, Name, OneFrame, CurrentParentWorldTransform, CurrentControlRigTransform);
							if (SnapSettings->bSnapPosition == false || SnapSettings->bSnapRotation == false || SnapSettings->bSnapScale == false)
							{
								FTransform& Transform = WorldTransformToSnap[Index];
								const FTransform& CurrentTransform = CurrentControlRigTransform[0];
								if (SnapSettings->bSnapPosition == false)
								{
									FVector CurrentPosition = CurrentTransform.GetLocation();
									Transform.SetLocation(CurrentPosition);
								}
								if (SnapSettings->bSnapRotation == false)
								{
									FQuat CurrentRotation = CurrentTransform.GetRotation();
									Transform.SetRotation(CurrentRotation);
								}
								if (SnapSettings->bSnapScale == false)
								{
									FVector Scale = CurrentTransform.GetScale3D();
									Transform.SetScale3D(Scale);
								}
								
							}
							const FFrameNumber& FrameNumber = Frames[Index];
							Context.LocalTime = TickResolution.AsSeconds(FFrameTime(FrameNumber));
							FTransform GlobalTransform = WorldTransformToSnap[Index].GetRelativeTransform(ControlRigParentWorldTransforms[Index]);
							ControlRig->SetControlGlobalTransform(Name, GlobalTransform, true, Context);
						}
					}
				}
			}
		}
	}
	return true;
}



#undef LOCTEXT_NAMESPACE



