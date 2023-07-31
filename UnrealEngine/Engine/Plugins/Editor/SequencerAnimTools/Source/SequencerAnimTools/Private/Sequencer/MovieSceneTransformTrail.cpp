// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTransformTrail.h"
#include "TrailHierarchy.h"

#include "ISequencer.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Systems/MovieSceneComponentTransformSystem.h"
#include "MovieSceneSequence.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"

#include "BaseGizmos/GizmoComponents.h"
#include "BaseGizmos/CombinedTransformGizmo.h"

#include "ViewportWorldInteraction.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "MovieSceneToolHelpers.h"
#include "ActorForWorldTransforms.h"
#include "IControlRigObjectBinding.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Tools/ControlRigSnapSettings.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SequencerAnimTools"

namespace UE
{
namespace SequencerAnimTools
{

/****************************************************************************************************************
*
*   Base Movie Scene Transform Trail
*
*****************************************************************************************************************/


FMovieSceneTransformTrail::FMovieSceneTransformTrail(USceneComponent* InOwner, const FLinearColor& InColor, const bool bInIsVisible, TWeakObjectPtr<UMovieSceneTrack> InWeakTrack, TSharedPtr<class ISequencer> InSequencer)
	: FTrail(InOwner)
	, WeakSequencer(InSequencer)
	, CachedEffectiveRange(TRange<double>::Empty())
	, bIsSelected(false)
	, TrajectoryCache()
	, LastTransformSectionSig(FGuid())
	, CachedHierarchyGuid()
	, WeakTrack(InWeakTrack)
{
	TrajectoryCache = MakeUnique<FArrayTrajectoryCache>(0.01, GetEffectiveSectionRange(0));
	KeyTool = MakeUnique< FMotionTraiMovieScenelKeyTool>(this);
	DrawInfo = MakeUnique<FTrajectoryDrawInfo>(InColor, TrajectoryCache.Get());
}

UMovieSceneSection* FMovieSceneTransformTrail::GetSection() const
{
	if (WeakTrack.IsValid())
	{
		UMovieSceneSection* Section = WeakTrack.Get()->GetSectionToKey();
		if (Section)
		{
			return Section;
		}
		else if( WeakTrack.Get()->GetAllSections().Num() >0)
		{
			return WeakTrack.Get()->GetAllSections()[0];
		}
	}
	return  nullptr;
}

bool FMovieSceneTransformTrail::GetEditedTimes(const FTrailHierarchy* TrailHierarchy,const FFrameNumber& LastFrame, TArray<FFrameNumber>& OutEditedTimes)
{
	if (EditedTimes.IsSet())
	{
		CalculateEditedTimes(TrailHierarchy,LastFrame);
		if (EditedTimes.IsSet())
		{
			OutEditedTimes = EditedTimes.GetValue();
			return true;
		}
	}
	return false;
}

void FMovieSceneTransformTrail::CalculateEditedTimes(const FTrailHierarchy* TrailHierarchy, const FFrameNumber& LastFrame)
{
	if (bIsSelected || KeyTool->IsAllSelected())
	{
		EditedTimes.Reset();
	}
	else
	{
		const FFrameNumber Step = TrailHierarchy->GetFramesPerSegment();
		TArray<FFrameNumber> SelectedTimes  = KeyTool->SelectedKeyTimes();
		EditedTimes = KeyTool->GetTimesFromModifiedTimes(SelectedTimes,LastFrame,Step);
		if (EditedTimes.GetValue().Num() == 0)
		{
			EditedTimes.Reset();
		}
	}
}

void FMovieSceneTransformTrail::ForceEvaluateNextTick()
{
	FTrail::ForceEvaluateNextTick();
	KeyTool->DirtyKeyTransforms();
}

void FMovieSceneTransformTrail::UpdateKeysInRange(const TRange<double>& ViewRange)
{
	KeyTool->DirtyKeyTransforms();
	KeyTool->UpdateKeysInRange(ViewRange);
}

ETrailCacheState FMovieSceneTransformTrail::UpdateTrail(const FSceneContext& InSceneContext)
{
	CachedHierarchyGuid = InSceneContext.YourNode;
	UMovieSceneSection* Section = GetSection();
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	FGuid SequencerBinding = FGuid();
	if (Sequencer && Section)
	{ // TODO: expensive, but for some reason Section stays alive even after it is deleted
		Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrackBinding(*Cast<UMovieSceneTrack>(Section->GetOuter()), SequencerBinding);
	}

	if (!Sequencer || !Section || !SequencerBinding.IsValid())
	{
		return ETrailCacheState::Dead;
	}
	
	const bool bTrackUnchanged = Section->GetSignature() == LastTransformSectionSig;

	if (!bTrackUnchanged  || bForceEvaluateNextTick)
	{

		KeyTool->OnSectionChanged();
		TrajectoryCache->UpdateCacheTimes(InSceneContext.EvalTimes);

		CacheState = ETrailCacheState::Stale;
		bForceEvaluateNextTick = false;
		LastTransformSectionSig = Section->GetSignature();

		UpdateCacheTimes(InSceneContext);
				
	}
	else 
	{
		TrajectoryCache->UpdateCacheTimes(InSceneContext.EvalTimes);
		KeyTool->UpdateSelectedKeysTransform();
		CacheState = ETrailCacheState::UpToDate;
	}
	return CacheState;
}

void FMovieSceneTransformTrail::DrawHUD(const FSceneView* View, FCanvas* Canvas)
{
	KeyTool->DrawHUD(View, Canvas);
}

void FMovieSceneTransformTrail::Render(const FGuid& Guid, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	KeyTool->Render(Guid, View, PDI);
}

bool FMovieSceneTransformTrail::HandleAltClick(FEditorViewportClient* InViewportClient, HMotionTrailProxy* Proxy, FInputClick Click)
{
	return false;
}

bool FMovieSceneTransformTrail::HandleClick(const FGuid& Guid, FEditorViewportClient* InViewportClient, HHitProxy* InHitProxy, FInputClick Click)
{
	bool bKeySelected = KeyTool->HandleClick(Guid,InViewportClient, InHitProxy, Click);
	if (bKeySelected)
	{
		if (Click.bShiftIsDown == false || Click.bCtrlIsDown == false)
		{
			bIsSelected = false;
		}
		return true;
	}

	if (HMotionTrailProxy* HitProxy = HitProxyCast<HMotionTrailProxy>(InHitProxy))
	{
		if (HitProxy->Guid == Guid)
		{

			//	FScopedTransaction ScopedTransaction(LOCTEXT("SelectControlTransaction", "Select Control"), IsInLevelEditor() && !GIsTransacting);
			if (Click.bAltIsDown)
			{
				return HandleAltClick(InViewportClient, HitProxy, Click);
			}
			if (Click.bShiftIsDown)
			{
				bIsSelected = true;
				SelectedPos = HitProxy->Point;
			}
			else if (Click.bCtrlIsDown)
			{
				if (bIsSelected)
				{
					bIsSelected = false;
				}
				else
				{
					bIsSelected = true;
					SelectedPos = HitProxy->Point;
				}
			}
			else
			{
				KeyTool->ClearSelection();
				bIsSelected = true;
				SelectedPos = HitProxy->Point;
			}
			return true;
		}
	}

	bIsSelected = false;
	return false;
}

bool FMovieSceneTransformTrail::IsAnythingSelected(FVector& OutVectorPosition) const
{
	if (bIsSelected)
	{
		OutVectorPosition = SelectedPos;
		return true;
	}
	return KeyTool->IsSelected(OutVectorPosition);
}

void FMovieSceneTransformTrail::TranslateSelectedKeys(bool bRight)
{
	KeyTool->TranslateSelectedKeys(bRight);
}

void FMovieSceneTransformTrail::DeleteSelectedKeys()
{
	KeyTool->DeleteSelectedKeys();
}

void FMovieSceneTransformTrail::SelectNone()
{
	KeyTool->ClearSelection();
}

bool FMovieSceneTransformTrail::IsAnythingSelected() const
{
	return (bIsSelected || KeyTool->IsSelected());
}

bool FMovieSceneTransformTrail::IsTrailSelected() const
{
	return bIsSelected;
}

bool FMovieSceneTransformTrail::StartTracking()
{
	return false;
}

bool FMovieSceneTransformTrail::ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation)
{
	return false;;
}

bool FMovieSceneTransformTrail::EndTracking()
{
	return false;
}

TArray<FFrameNumber> FMovieSceneTransformTrail::GetKeyTimes() const
{
	TArray<FTrailKeyInfo*> AllKeys;
	KeyTool->GetAllKeys(AllKeys);

	TArray<FFrameNumber> Frames;
	for (FTrailKeyInfo* Info : AllKeys)
	{
		if (Info)
		{
			Frames.Add(Info->FrameNumber);
		}
	}
	return Frames;
}

TArray<FFrameNumber> FMovieSceneTransformTrail::GetSelectedKeyTimes() const
{
	return KeyTool->SelectedKeyTimes();
}

TRange<double> FMovieSceneTransformTrail::GetEffectiveSectionRange(int32 ChannelOffset) const
{
	UMovieSceneSection* TransformSection = GetSection();
	if (!TransformSection)
	{
		return TRange<double>();
	}
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(TransformSection && Sequencer);

	TRange<FFrameNumber> EffectiveRange = TRange<FFrameNumber>::Empty();

	int32 MaxChannel = (int32)ETransformChannel::TranslateZ; //0,1,2 are the position channels only onces we care about.
	TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
	TArrayView<FMovieSceneFloatChannel*> FloatChannels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

	if (DoubleChannels.Num() > MaxChannel)
	{
		for (int32 ChannelIdx = ChannelOffset; ChannelIdx <= (ChannelOffset + MaxChannel); ChannelIdx++)
		{
			FMovieSceneDoubleChannel* Channel = DoubleChannels[ChannelIdx];
			EffectiveRange = TRange<FFrameNumber>::Hull(EffectiveRange, Channel->ComputeEffectiveRange());
		}
	}
	if (FloatChannels.Num() > MaxChannel)
	{
		for (int32 ChannelIdx = ChannelOffset; ChannelIdx <= (ChannelOffset + MaxChannel); ChannelIdx++)
		{
			FMovieSceneFloatChannel* Channel = FloatChannels[ChannelIdx];
			EffectiveRange = TRange<FFrameNumber>::Hull(EffectiveRange, Channel->ComputeEffectiveRange());
		}
	}

	EffectiveRange = TRange<FFrameNumber>::Intersection(EffectiveRange, TransformSection->GetRange());

	TRange<double> SectionRangeSeconds = TRange<double>(
		Sequencer->GetFocusedTickResolution().AsSeconds(EffectiveRange.GetLowerBoundValue()),
		Sequencer->GetFocusedTickResolution().AsSeconds(EffectiveRange.GetUpperBoundValue())
	);
		
	// TODO: clip by movie scene range? try movie keys outside of movie scene range

	return SectionRangeSeconds;
}

/****************************************************************************************************************
*
*	Movie Scene Transform Component Trail
*
*****************************************************************************************************************/


void FMovieSceneComponentTransformTrail::UpdateCacheTimes(const FSceneContext& InSceneContext)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(Sequencer);
	FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
	const FTrailEvaluateTimes& EvaluateTimes = InSceneContext.EvalTimes;
	TArray<FFrameNumber> Frames;
	bool bUseEditedTimes = false;
	if (EditedTimes.IsSet())
	{
		const FFrameTime LastFrame = EvaluateTimes.EvalTimes[EvaluateTimes.EvalTimes.Num() - 1] * TickResolution;

		CalculateEditedTimes(InSceneContext.TrailHierarchy,LastFrame.RoundToFrame());
		Frames = EditedTimes.GetValue();
		bUseEditedTimes = true;
	}
	else
	{
		//Todo mz make time in frames
		int32 Index = 0;
		Frames.SetNum(EvaluateTimes.EvalTimes.Num());
		for (const double Time : EvaluateTimes.EvalTimes)
		{
			const FFrameTime TickTime = Time * TickResolution;
			Frames[Index++] = TickTime.RoundToFrame();
		}
	}

	TArray<FTransform> OutWorldTransforms;
	FActorForWorldTransforms Actors;
	Actors.Actor = Component->GetTypedOuter<AActor>();
	Actors.Component = Component;
	MovieSceneToolHelpers::GetActorWorldTransforms(GetSequencer().Get(), Actors, Frames, OutWorldTransforms);
	TArray<FTransform> ParentCache;
	if (EditedTimes.IsSet())
	{
		for (int32 Index = 0; Index < Frames.Num(); ++Index)
		{
			const FFrameNumber& FrameNumber = Frames[Index];
			double Sec = TickResolution.AsSeconds(FFrameTime(FrameNumber));
			GetTrajectoryTransforms()->Set(Sec, OutWorldTransforms[Index]);
		}
	}
	else
	{
		GetTrajectoryTransforms()->SetTransforms(OutWorldTransforms, ParentCache);
	}

	KeyTool->UpdateKeysInRange(InSceneContext.TrailHierarchy->GetViewRange());
	EditedTimes.Reset();
}

bool FMovieSceneComponentTransformTrail::StartTracking()
{
	UMovieSceneSection* Section = GetSection();
	if (!Section)
	{
		return false;
	}

	if (IsAnythingSelected())
	{
		EditedTimes.Reset();
		Section->Modify();
		KeyTool->StartDragging();
		return true;
	}
	return false;
}

static bool SetActorTransform(ISequencer* Sequencer, AActor *Actor, UMovieScene3DTransformSection* TransformSection, const TArray<FFrameNumber>& Frames,
	const TArray<FTransform>& WorldTransformsToSnapTo)
{
	if (!Sequencer || !Sequencer->GetFocusedMovieSceneSequence())
	{
		return false;
	}
	if (!TransformSection)
	{
		return false;
	}

	TransformSection->Modify();

	TArray<FTransform> ParentWorldTransforms;
	AActor* ParentActor = Actor->GetAttachParentActor();
	if (ParentActor)
	{
		FActorForWorldTransforms ActorSelection;
		ActorSelection.Actor = ParentActor;
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

	TArrayView<FMovieSceneDoubleChannel*> Channels = TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
	for (int32 Index = 0; Index < Frames.Num(); ++Index)
	{
		const FFrameNumber& Frame = Frames[Index];
		FTransform ParentTransform = ParentWorldTransforms[Index];
		FTransform WorldTransform = WorldTransformsToSnapTo[Index];
		FTransform LocalTransform = WorldTransform.GetRelativeTransform(ParentTransform);
		FVector Location = LocalTransform.GetLocation();
		FRotator Rotation = LocalTransform.GetRotation().Rotator();
			
		TMovieSceneChannelData<FMovieSceneDoubleValue> ChannelData = Channels[0]->GetData();
		MovieSceneToolHelpers::SetOrAddKey(ChannelData, Frame, Location.X);
		ChannelData = Channels[1]->GetData();
		MovieSceneToolHelpers::SetOrAddKey(ChannelData, Frame, Location.Y);
		ChannelData = Channels[2]->GetData();
		MovieSceneToolHelpers::SetOrAddKey(ChannelData, Frame, Location.Z);			
	}

	Channels[0]->AutoSetTangents();
	Channels[1]->AutoSetTangents();
	Channels[2]->AutoSetTangents();
		
	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
	return true;
}

bool FMovieSceneComponentTransformTrail::ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation)
{
	if (Component.IsValid() == false)
	{
		return false;
	}
	UMovieSceneSection* Section = GetSection();
	if (!Section)
	{
		return false;
	}
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);
	if (!Section)
	{
		return false;
	}

	if (Pos.IsNearlyZero() && Rot.IsNearlyZero())
	{
		return false;
	}

	if (IsAnythingSelected())
	{
		TArray<FTransform> Transforms;
		TArray<FFrameNumber> Times;
		if (IsTrailSelected())
		{
			TArray<FTrailKeyInfo*> Keys;
			KeyTool->GetAllKeys(Keys);
			for (FTrailKeyInfo* KeyInfo : Keys)
			{
				FTransform NewTransform(KeyInfo->Transform);
				NewTransform.SetLocation(NewTransform.GetLocation() + Pos);
				KeyInfo->Transform = NewTransform;
				Transforms.Add(NewTransform);
				Times.Add(KeyInfo->FrameNumber);
			}
		}
		else
		{
			for (FTrailKeyInfo* KeyInfo : KeyTool->CachedSelection)
			{
				FTransform NewTransform(KeyInfo->Transform);
				NewTransform.SetLocation(NewTransform.GetLocation() + Pos);
				KeyInfo->Transform = NewTransform;
				Transforms.Add(NewTransform);
				Times.Add(KeyInfo->FrameNumber);
			}
			TArray<FFrameNumber> EditTimesSet;
			EditedTimes = EditTimesSet;
		}
		AActor* Actor = Component.Get()->GetTypedOuter<AActor>();
		SetActorTransform(GetSequencer().Get(), Actor, TransformSection, Times, Transforms);
		KeyTool->UpdateSelectedKeysTransform();
		return true;
	}
	return false;
}

bool FMovieSceneComponentTransformTrail::EndTracking()
{
	EditedTimes.Reset();
	UMovieSceneSection* Section = GetSection();
	if (!Section)
	{
		return false;
	}

	if (IsAnythingSelected())
	{
		//need to broadcast this so we can update other systems(including our own).
		AActor* Actor = Component.Get()->GetTypedOuter<AActor>();
		GEngine->BroadcastOnActorMoved(Actor);
		return true;
	}
	return false;
}

bool FMovieSceneComponentTransformTrail::HandleAltClick( FEditorViewportClient* InViewportClient, HMotionTrailProxy* Proxy,FInputClick Click)
{
	if (Component.IsValid() == false)
	{
		return false;
	}
	UMovieSceneSection* Section = GetSection();
	if (!Section)
	{
		return false;
	}
	UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);
	if (!Section)
	{
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("InsertKey", "Insert Key"));

	TArray<FFrameNumber> Times;
	TArray<FTransform> Transforms;
	FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();
	FFrameRate DisplayResolution = GetSequencer()->GetFocusedDisplayRate();
	//Snap to Frame. Assuming we want keys on frames.
	FFrameTime GlobalTime = TickResolution.AsFrameTime(Proxy->Second);
	FFrameTime DisplayTime = FFrameRate::TransformTime(GlobalTime, DisplayResolution, TickResolution);
	DisplayTime = DisplayTime.RoundToFrame();
	FFrameTime EvalTime = FFrameRate::TransformTime(DisplayTime, TickResolution, DisplayResolution);
	Times.Add(EvalTime.RoundToFrame());
	const FTransform Transform = GetTrajectoryTransforms()->GetInterp(Proxy->Second);
	Transforms.Add(Transform);
	AActor* Actor = Component.Get()->GetTypedOuter<AActor>();
	SetActorTransform(GetSequencer().Get(), Actor, TransformSection, Times, Transforms);
	KeyTool->UpdateSelectedKeysTransform();
	return true;
}

/****************************************************************************************************************
*
*   Control Rig Transform Trail
*
*****************************************************************************************************************/

FMovieSceneControlRigTransformTrail::FMovieSceneControlRigTransformTrail(USceneComponent* SceneComponent, const FLinearColor& InColor, const bool bInIsVisible, TWeakObjectPtr<UMovieSceneTrack> InWeakTrack, TSharedPtr<ISequencer> InSequencer,  const FName& InControlName)
: FMovieSceneTransformTrail(SceneComponent, InColor, bInIsVisible, InWeakTrack, InSequencer)
, ControlName(InControlName)
{
}


void FMovieSceneControlRigTransformTrail::UpdateCacheTimes(const FSceneContext& InSceneContext)
{
	/*
	We don't do this anymore since we instead update the cache for EVERY drawn control rig trail otherwise
	we evaluate the rig again for each trail.
	*/
}

void FMovieSceneControlRigTransformTrail::SetUseKeysForTrajectory(bool bVal)
{
	bUseKeysForTrajectory = bVal;
}

void FMovieSceneControlRigTransformTrail::GetTrajectoryPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector>& OutPoints, TArray<double>& OutSeconds)
{
	if (FTrajectoryDrawInfo* DI = GetDrawInfo())
	{
		if (bUseKeysForTrajectory == false)
		{
			DI->GetTrajectoryPointsForDisplay(InDisplayContext, OutPoints, OutSeconds);
		}
		else
		{
			TRange<double> TimeRange = InDisplayContext.TimeRange;

			TArray<FTrailKeyInfo*> AllKeys;
			KeyTool->GetAllKeys(AllKeys);
			TSharedPtr<ISequencer> SequencerPtr = WeakSequencer.Pin();
			FFrameNumber CurrentFrame = SequencerPtr->GetLocalTime().Time.GetFrame();
			FFrameRate TickResolution = SequencerPtr->GetFocusedTickResolution();
			bool bCurrentHandled = false;
			//get cached positions from all of the keys and the current frame if it's between two keys
			for (int32 Index = 0; Index < AllKeys.Num(); ++Index)
			{
				if (FTrailKeyInfo* KeyInfo = AllKeys[Index])
				{
					double LocalTime = TickResolution.AsSeconds(FFrameTime(KeyInfo->FrameNumber));
					if (TimeRange.Contains(LocalTime))
					{
						FVector Point = DI->GetPoint(LocalTime);
						OutPoints.Add(Point);
						OutSeconds.Add(LocalTime);
					}
					if (LocalTime > TimeRange.GetUpperBoundValue())
					{
						break;
					}
					if (bCurrentHandled == false && ((Index + 1) < AllKeys.Num()))
					{
						if (CurrentFrame > KeyInfo->FrameNumber)
						{
							if (FTrailKeyInfo* NextKeyInfo = AllKeys[Index + 1])
							{
								if (CurrentFrame < NextKeyInfo->FrameNumber)
								{
									double CurrentTime = TickResolution.AsSeconds(FFrameTime(CurrentFrame));
									if (TimeRange.Contains(CurrentTime))
									{
										FVector Point = DI->GetPoint(CurrentTime);
										OutPoints.Add(Point);
										OutSeconds.Add(CurrentTime);
										bCurrentHandled = true;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void FMovieSceneControlRigTransformTrail::GetTickPointsForDisplay(const FDisplayContext& InDisplayContext, TArray<FVector2D>& OutTicks, TArray<FVector2D>& OutTickTangents)
{
	if (bUseKeysForTrajectory == false)
	{
		if (FTrajectoryDrawInfo* DI = GetDrawInfo())
		{
			DI->GetTickPointsForDisplay(InDisplayContext, OutTicks, OutTickTangents);
		}
	}
}


bool FMovieSceneControlRigTransformTrail::StartTracking()
{
	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(GetSection());
	if (!Section)
	{
		return false;
	}
	UControlRig* ControlRig = Section->GetControlRig();
	if (!ControlRig)
	{
		return false ;
	}
	if(IsAnythingSelected())
	{ 
		EditedTimes.Reset();
		Section->Modify();
		ControlRig->Modify();
		KeyTool->StartDragging();
		return true;
	}
	return false;
}

int32 FMovieSceneControlRigTransformTrail::GetChannelOffset() const
{
	UMovieSceneControlRigParameterSection* CRParamSection = Cast<UMovieSceneControlRigParameterSection>(GetSection());
	if (CRParamSection)
	{
		FChannelMapInfo* pChannelIndex = CRParamSection->ControlChannelMap.Find(ControlName);
		return pChannelIndex ? pChannelIndex->ChannelIndex : INDEX_NONE;
	}
	return INDEX_NONE;
}

bool FMovieSceneControlRigTransformTrail::ApplyDelta(const FVector& Pos, const FRotator& Rot, const FVector& WidgetLocation)
{
	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(GetSection());
	if (!Section)
	{
		return false;
	}
	UControlRig* ControlRig = Section->GetControlRig();
	if (!ControlRig)
	{
		return false;
	}

	if (Pos.IsNearlyZero() && Rot.IsNearlyZero())
	{
		return false;
	}

	if (IsAnythingSelected())
	{
		FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		IMovieScenePlayer* Player = GetSequencer().Get();

		FMovieSceneSequenceTransform RootToLocalTransform = GetSequencer()->GetFocusedMovieSceneSequenceTransform();

		auto EvalControlRig = [&Context,&RootToLocalTransform, Pos, TickResolution, ControlRig,Player, this](FTrailKeyInfo* KeyInfo)
		{
			if (KeyInfo)
			{
				Context.LocalTime = TickResolution.AsSeconds(FFrameTime(KeyInfo->FrameNumber));
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::Translation;
				FFrameTime GlobalTime(KeyInfo->FrameNumber);
				GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly(); //player evals in root time so need to go back to it.

				FMovieSceneContext MovieSceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);

				FTransform NewTransform(KeyInfo->Transform);
				NewTransform.SetLocation(NewTransform.GetLocation() + Pos);
				KeyInfo->Transform = NewTransform;
				NewTransform = NewTransform.GetRelativeTransform(KeyInfo->ParentTransform);
				GetSequencer()->GetEvaluationTemplate().EvaluateSynchronousBlocking(MovieSceneContext, *Player);
				ControlRig->Evaluate_AnyThread();
				ControlRig->SetControlGlobalTransform(ControlName, NewTransform, true, Context, false);
			}
		};

		if (IsTrailSelected())
		{
			TArray<FTrailKeyInfo*> Keys;
			KeyTool->GetAllKeys(Keys);
			for (FTrailKeyInfo* KeyInfo : Keys)
			{
				EvalControlRig(KeyInfo);
			}
			SelectedPos += Pos;
		}
		else
		{
			for (FTrailKeyInfo* KeyInfo : KeyTool->CachedSelection)
			{
				EvalControlRig(KeyInfo);
			}
			TArray<FFrameNumber> EditTimesSet;
			EditedTimes = EditTimesSet;
		}
		KeyTool->UpdateSelectedKeysTransform();
		return true;
	}
	return false;
}

bool FMovieSceneControlRigTransformTrail::EndTracking()
{
	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(GetSection());
	if (!Section)
	{
		return false;
	}
	UControlRig* ControlRig = Section->GetControlRig();
	if (!ControlRig)
	{
		return false;
	}
	EditedTimes.Reset();

	if (IsAnythingSelected())
	{
		IMovieScenePlayer* Player = GetSequencer().Get();
		FFrameTime StartTime = GetSequencer()->GetLocalTime().Time;
		FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();
		FMovieSceneSequenceTransform RootToLocalTransform = GetSequencer()->GetFocusedMovieSceneSequenceTransform();
		StartTime = StartTime * RootToLocalTransform.InverseLinearOnly(); //player evals in root time so need to go back to it.

		FMovieSceneContext MovieSceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(StartTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);
		
		Player->GetEvaluationTemplate().EvaluateSynchronousBlocking(MovieSceneContext, *Player);
		ControlRig->Evaluate_AnyThread();
		return true;
	}
	return false;
}

bool FMovieSceneControlRigTransformTrail::HandleAltClick(FEditorViewportClient* InViewportClient, HMotionTrailProxy* Proxy, FInputClick Click)
{
	UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(GetSection());
	if (!Section)
	{
		return false;
	}
	UControlRig* ControlRig = Section->GetControlRig();
	if (!ControlRig)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("InsertKey", "Insert Key"));
	Section->Modify();
	ControlRig->Modify();
	FFrameRate TickResolution = GetSequencer()->GetFocusedTickResolution();
	FFrameRate DisplayResolution = GetSequencer()->GetFocusedDisplayRate();

	FRigControlModifiedContext Context;
	Context.SetKey = EControlRigSetKey::Always;
	IMovieScenePlayer* Player = GetSequencer().Get();

	//Snap to Frame. Assuming we want keys on frames.
	FFrameTime GlobalTime = TickResolution.AsFrameTime(Proxy->Second);
	FFrameTime DisplayTime = FFrameRate::TransformTime(GlobalTime, DisplayResolution, TickResolution);
	DisplayTime = DisplayTime.RoundToFrame();
	GlobalTime = FFrameRate::TransformTime(DisplayTime, TickResolution, DisplayResolution);
	FMovieSceneSequenceTransform RootToLocalTransform = GetSequencer()->GetFocusedMovieSceneSequenceTransform();
	GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly(); //player evals in root time so need to go back to it.

	Context.LocalTime = TickResolution.AsSeconds(GlobalTime);
	Context.KeyMask = (uint32)EControlRigContextChannelToKey::Translation;
	FMovieSceneContext MovieSceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);
	GetSequencer()->GetEvaluationTemplate().EvaluateSynchronousBlocking(MovieSceneContext, *Player);
	ControlRig->Evaluate_AnyThread();
	FTransform NewTransform(ControlRig->GetControlGlobalTransform(ControlName));
	ControlRig->SetControlGlobalTransform(ControlName, NewTransform, true, Context, false);

	//eval back at current time
	FFrameTime StartTime = GetSequencer()->GetLocalTime().Time;
	StartTime = StartTime * RootToLocalTransform.InverseLinearOnly(); //player evals in root time so need to go back to it.

	MovieSceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(StartTime, TickResolution), Player->GetPlaybackStatus()).SetHasJumped(true);
	Player->GetEvaluationTemplate().EvaluateSynchronousBlocking(MovieSceneContext, *Player);
	ControlRig->Evaluate_AnyThread();

	//create new keys
	KeyTool->BuildKeys();
	FTrailKeyInfo* KeyInfo = KeyTool->FindKey(GlobalTime.RoundToFrame());
	if (KeyInfo)
	{
		if (Click.bShiftIsDown == false && Click.bCtrlIsDown == false)
		{
			KeyTool->ClearSelection();;
		}
		KeyTool->SelectKeyInfo(KeyInfo);
	}
	return true;
}



} // namespace MovieScene
} // namespace UE

#undef LOCTEXT_NAMESPACE
