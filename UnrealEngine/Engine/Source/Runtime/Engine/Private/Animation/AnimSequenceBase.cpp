// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSequenceBase.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifyEndDataContext.h"
#include "Animation/AnimInstance.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "UObject/FrameworkObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Modules/ModuleManager.h"
#include "MathUtil.h"

#if WITH_EDITOR
#include "IAnimationDataControllerModule.h"
#include "Modules/ModuleManager.h"
#endif // WITH_EDITOR

#include "UObject/UE5MainStreamObjectVersion.h"

DEFINE_LOG_CATEGORY(LogAnimMarkerSync);
CSV_DECLARE_CATEGORY_MODULE_EXTERN(ENGINE_API, Animation);

#define LOCTEXT_NAMESPACE "AnimSequenceBase"
/////////////////////////////////////////////////////

UAnimSequenceBase::UAnimSequenceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RateScale(1.0f)
	, bLoop(false)
#if WITH_EDITORONLY_DATA
	, DataModel(nullptr)
	, bPopulatingDataModel(false)
	, Controller(nullptr)
#endif // WITH_EDITORONLY_DATA
{
#if WITH_EDITOR
	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject| EObjectFlags::RF_NeedLoad))
	{
		CreateModel();
		GetController();
	}
#endif // WITH_EDITOR
}

void UAnimSequenceBase::SetSequenceLength(float NewLength)
{
#if WITH_EDITOR
	// Editor time we update the source data
	Controller->SetPlayLength(NewLength);
#else 
	// In case this is called during runtime, update the sequence length directly. Current use-case is runtime created Montages.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SequenceLength = NewLength;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

bool UAnimSequenceBase::IsPostLoadThreadSafe() const
{
	return false;	// PostLoad is not thread safe because of the call to VerifyCurveNames() (calling USkeleton::VerifySmartName) that can mutate a shared map in the skeleton.
}

#if WITH_EDITORONLY_DATA
void UAnimSequenceBase::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);

	OutConstructClasses.Add(FTopLevelAssetPath(TEXT("/Script/Engine.AnimDataModel")));

	// We need to declare all types that can be returned from UE::Anim::DataModel::IAnimationDataModels::FindClassForAnimationAsset(UAnimSequenceBase*);
	OutConstructClasses.Add(FTopLevelAssetPath(TEXT("/Script/AnimationData.AnimationSequencerDataModel")));

	// We can call Controller->CreateModel, which can add objects, so add every ControllerClass as something we can construct.
	// THe caller will add on all recursively constructable classes
	UClass* AnimationControllerClass = UAnimationDataController::StaticClass();
	for (TObjectIterator<UClass> Iter; Iter; ++Iter)
	{
		if ((*Iter)->ImplementsInterface(AnimationControllerClass))
		{
			OutConstructClasses.Add(FTopLevelAssetPath(*Iter));
		}
	}
}
#endif

void UAnimSequenceBase::PostLoad()
{
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		auto PreloadSkeletonAndVerifyCurves = [this]()
		{
			if (USkeleton* MySkeleton = GetSkeleton())
			{
				if (FLinkerLoad* SkeletonLinker = MySkeleton->GetLinker())
				{
					SkeletonLinker->Preload(MySkeleton);
				}
				MySkeleton->ConditionalPostLoad();

				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				VerifyCurveNames<FFloatCurve>(*MySkeleton, USkeleton::AnimCurveMappingName, RawCurveData.FloatCurves);
				VerifyCurveNames<FTransformCurve>(*MySkeleton, USkeleton::AnimTrackCurveMappingName, RawCurveData.TransformCurves);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		};
		
		if(ShouldDataModelBeValid())
		{
			const bool bRequiresModelCreation = DataModel == nullptr;
			const bool bRequiresModelPopulation = GetLinkerCustomVersion(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::IntroducingAnimationDataModel;
			checkf(bRequiresModelPopulation || DataModel != nullptr, TEXT("Invalid Animation Sequence base state, no data model found past upgrade object version"));

			// Construct a new UAnimDataModel instance
			if(bRequiresModelCreation)
			{
				CreateModel();
			}

			ValidateModel();
			GetController();
			BindToModelModificationEvent();

			PreloadSkeletonAndVerifyCurves();

			GetController();
			if (bRequiresModelPopulation)
			{
				bPopulatingDataModel = true;
				PopulateModel();
				bPopulatingDataModel = false;
			}
		}
		else
		{
			PreloadSkeletonAndVerifyCurves();
		}
	}
#endif // WITH_EDITORONLY_DATA

	Super::PostLoad();

	// Convert Notifies to new data
	if( GIsEditor && Notifies.Num() > 0 )
	{
		if(GetLinkerUEVersion() < VER_UE4_CLEAR_NOTIFY_TRIGGERS)
		{
			for(FAnimNotifyEvent Notify : Notifies)
			{
				if(Notify.Notify)
				{
					// Clear end triggers for notifies that are not notify states
					Notify.EndTriggerTimeOffset = 0.0f;
				}
			}
		}
	}

#if WITH_EDITOR
	InitializeNotifyTrack();
#endif
	RefreshCacheData();

	if(USkeleton* MySkeleton = GetSkeleton())
	{
		const bool bDoNotTransactAction = false;
#if WITH_EDITOR
		if (IsDataModelValid())
		{
			if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::FixUpNoneNameAnimationCurves)
			{
				
				Controller->OpenBracket(LOCTEXT("FFortniteMainBranchObjectVersion::FixUpNoneNameAnimationCurves_Bracket","FFortniteMainBranchObjectVersion::FixUpNoneNameAnimationCurves"), bDoNotTransactAction);
				{
					const TArray<FFloatCurve>& FloatCurves = DataModel->GetFloatCurves();
					for (int32 Index = 0; Index < FloatCurves.Num(); ++Index)
					{
						const FFloatCurve& Curve = FloatCurves[Index];
						if (Curve.Name.DisplayName == NAME_None)
						{
							// give unique name
							const FName UniqueName = FName(*FString(GetName() + TEXT("_CurveNameFix_") + FString::FromInt(Index)));
							UE_LOG(LogAnimation, Warning, TEXT("[AnimSequence %s] contains invalid curve name \'None\'. Renaming this to %s. Please fix this curve in the editor. "), *GetFullName(), *Curve.Name.DisplayName.ToString());

							FSmartName NewSmartName = Curve.Name;
							NewSmartName.DisplayName = UniqueName;

							Controller->RenameCurve(FAnimationCurveIdentifier(Curve.Name, ERawCurveTrackTypes::RCT_Float), FAnimationCurveIdentifier(NewSmartName, ERawCurveTrackTypes::RCT_Float), bDoNotTransactAction);
						}
					}
				}
				Controller->CloseBracket(bDoNotTransactAction);
			}
			else
			{
				const TArray<FFloatCurve>& FloatCurves = DataModel->GetFloatCurves();
				for (int32 Index = 0; Index < FloatCurves.Num(); ++Index)
				{
					const FFloatCurve& Curve = FloatCurves[Index];
					ensureMsgf(Curve.Name.DisplayName != NAME_None, TEXT("[AnimSequencer %s] has invalid curve name."), *GetFullName());
				}
			}

			Controller->FindOrAddCurveNamesOnSkeleton(MySkeleton, ERawCurveTrackTypes::RCT_Float, bDoNotTransactAction);
			Controller->FindOrAddCurveNamesOnSkeleton(MySkeleton, ERawCurveTrackTypes::RCT_Transform, bDoNotTransactAction);
		}
#else
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		VerifyCurveNames<FFloatCurve>(*MySkeleton, USkeleton::AnimCurveMappingName, RawCurveData.FloatCurves);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

		// this should continue to add if skeleton hasn't been saved either 
		// we don't wipe out data, so make sure you add back in if required
		if (GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MoveCurveTypesToSkeleton
			|| MySkeleton->GetLinkerCustomVersion(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::MoveCurveTypesToSkeleton)
		{

#if WITH_EDITOR
			const TArray<FFloatCurve>& FloatCurves = DataModel->GetFloatCurves();
#else
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			const TArray<FFloatCurve>& FloatCurves = RawCurveData.FloatCurves;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
			// fix up curve flags to skeleton
			for (const FFloatCurve& Curve : FloatCurves)
			{
				bool bMorphtargetSet = Curve.GetCurveTypeFlag(AACF_DriveMorphTarget_DEPRECATED);
				bool bMaterialSet = Curve.GetCurveTypeFlag(AACF_DriveMaterial_DEPRECATED);

				// only add this if that has to 
				if (bMorphtargetSet || bMaterialSet)
				{
					MySkeleton->AccumulateCurveMetaData(Curve.Name.DisplayName, bMaterialSet, bMorphtargetSet);
				}
			}
		}
	}
}

void UAnimSequenceBase::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

#if WITH_EDITOR
	if (IsDataModelValid())
	{
		BindToModelModificationEvent();
	}
#endif // WITH_EDITOR
}

float UAnimSequenceBase::GetPlayLength() const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return SequenceLength;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UAnimSequenceBase::SortNotifies()
{
	// Sorts using FAnimNotifyEvent::operator<()
	Notifies.Sort();
}

bool UAnimSequenceBase::RemoveNotifies(const TArray<FName>& NotifiesToRemove)
{
	bool bSequenceModified = false;
	for (int32 NotifyIndex = Notifies.Num() - 1; NotifyIndex >= 0; --NotifyIndex)
	{
		FAnimNotifyEvent& AnimNotify = Notifies[NotifyIndex];
		if (NotifiesToRemove.Contains(AnimNotify.NotifyName))
		{
			if (!bSequenceModified)
			{
				Modify();
				bSequenceModified = true;
			}
			Notifies.RemoveAtSwap(NotifyIndex);
		}
	}

	if (bSequenceModified)
	{
		MarkPackageDirty();
		RefreshCacheData();
	}
	return bSequenceModified;
}

void UAnimSequenceBase::RemoveNotifies()
{
	if (Notifies.Num() == 0)
	{
		return;
	}
	Modify();
	Notifies.Reset();
	MarkPackageDirty();
	RefreshCacheData();
}

bool UAnimSequenceBase::IsNotifyAvailable() const
{
	return (Notifies.Num() != 0) && (GetPlayLength() > 0.f);
}

void UAnimSequenceBase::GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const
{
	FAnimTickRecord TickRecord;
	TickRecord.bLooping = bAllowLooping; 
	FAnimNotifyContext NotifyContext(TickRecord);
	GetAnimNotifies(StartTime, DeltaTime, NotifyContext);

	OutActiveNotifies.Reset(NotifyContext.ActiveNotifies.Num());
	for (FAnimNotifyEventReference NotifyRef : NotifyContext.ActiveNotifies)
	{
		if (const FAnimNotifyEvent* Notify = NotifyRef.GetNotify())
		{
			OutActiveNotifies.Add(Notify);
		}
	}
}

void UAnimSequenceBase::GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<FAnimNotifyEventReference>& OutActiveNotifies) const
{
	FAnimTickRecord TickRecord;
	TickRecord.bLooping = bAllowLooping;
	FAnimNotifyContext NotifyContext(TickRecord);
	GetAnimNotifies(StartTime, DeltaTime, NotifyContext);
	Swap(NotifyContext.ActiveNotifies, OutActiveNotifies);
}

void UAnimSequenceBase::GetAnimNotifies(const float& StartTime, const float& DeltaTime, FAnimNotifyContext& NotifyContext) const
{
	if(DeltaTime == 0.f)
	{
		return;
	}

	// Early out if we have no notifies
	if (!IsNotifyAvailable())
	{
		return;
	}
	
	bool const bPlayingBackwards = (DeltaTime < 0.f);
	float PreviousPosition = StartTime;
	float CurrentPosition = StartTime;
	float DesiredDeltaMove = DeltaTime;

	do 
	{
		// Disable looping here. Advance to desired position, or beginning / end of animation
		const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, GetPlayLength());

		// Verify position assumptions
		ensureMsgf(bPlayingBackwards ? (CurrentPosition <= PreviousPosition) : (CurrentPosition >= PreviousPosition), TEXT("in Animation %s(Skeleton %s) : bPlayingBackwards(%d), PreviousPosition(%0.2f), Current Position(%0.2f)"), 
			*GetName(), *GetNameSafe(GetSkeleton()), bPlayingBackwards, PreviousPosition, CurrentPosition);
		
		GetAnimNotifiesFromDeltaPositions(PreviousPosition, CurrentPosition, NotifyContext);
	
		// If we've hit the end of the animation, and we're allowed to loop, keep going.
		if( (AdvanceType == ETAA_Finished) &&  NotifyContext.TickRecord && NotifyContext.TickRecord->bLooping )
		{
			const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
			DesiredDeltaMove -= ActualDeltaMove; 

			PreviousPosition = bPlayingBackwards ? GetPlayLength() : 0.f;
			CurrentPosition = PreviousPosition;
		}
		else
		{
			break;
		}
	} 
	while( true );
}

void UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const
{
	FAnimTickRecord TickRecord;
	FAnimNotifyContext NotifyContext(TickRecord);
	GetAnimNotifiesFromDeltaPositions(PreviousPosition, CurrentPosition, NotifyContext);

	OutActiveNotifies.Reset(NotifyContext.ActiveNotifies.Num());
	for (FAnimNotifyEventReference NotifyRef : NotifyContext.ActiveNotifies)
	{
		if (const FAnimNotifyEvent* Notify = NotifyRef.GetNotify())
		{
			OutActiveNotifies.Add(Notify);
		}
	}
}

void UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float & CurrentPosition, TArray<FAnimNotifyEventReference>& OutActiveNotifies) const
{
	FAnimTickRecord TickRecord;
	FAnimNotifyContext NotifyContext(TickRecord);
	GetAnimNotifiesFromDeltaPositions(PreviousPosition, CurrentPosition,NotifyContext);
	Swap(NotifyContext.ActiveNotifies, OutActiveNotifies);
}

void UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition,  FAnimNotifyContext& NotifyContext) const
{
	// Early out if we have no notifies
	if( (Notifies.Num() == 0) || (PreviousPosition == CurrentPosition) )
	{
		return;
	}

	bool const bPlayingBackwards = (CurrentPosition < PreviousPosition);

	// If playing backwards, flip Min and Max.
	if( bPlayingBackwards )
	{
		for (int32 NotifyIndex=0; NotifyIndex<Notifies.Num(); NotifyIndex++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = Notifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.GetTriggerTime();
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			if( (NotifyStartTime < PreviousPosition) && (NotifyEndTime >= CurrentPosition) )
			{
				if (NotifyContext.TickRecord)
				{
					NotifyContext.ActiveNotifies.Emplace(&AnimNotifyEvent, this, NotifyContext.TickRecord->MirrorDataTable);
					NotifyContext.ActiveNotifies.Top().GatherTickRecordData(*NotifyContext.TickRecord);
				}
				else
				{
					NotifyContext.ActiveNotifies.Emplace(&AnimNotifyEvent, this, nullptr);
				}

				const bool bHasFinished = CurrentPosition <= FMathf::Max(NotifyStartTime, 0.f);
				if (bHasFinished)
				{
					NotifyContext.ActiveNotifies.Top().AddContextData<UE::Anim::FAnimNotifyEndDataContext>(true);
				}
			}
		}
	}
	else
	{
		for (int32 NotifyIndex=0; NotifyIndex<Notifies.Num(); NotifyIndex++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = Notifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.GetTriggerTime();
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			if( (NotifyStartTime <= CurrentPosition) && (NotifyEndTime > PreviousPosition) )
			{
				if (NotifyContext.TickRecord)
				{
					NotifyContext.ActiveNotifies.Emplace(&AnimNotifyEvent, this, NotifyContext.TickRecord->MirrorDataTable);
					NotifyContext.ActiveNotifies.Top().GatherTickRecordData(*NotifyContext.TickRecord); 
				}
				else
				{
					NotifyContext.ActiveNotifies.Emplace(&AnimNotifyEvent, this, nullptr);
				}

				const bool bHasFinished = CurrentPosition >= NotifyEndTime;
				if (bHasFinished)
				{
					NotifyContext.ActiveNotifies.Top().AddContextData<UE::Anim::FAnimNotifyEndDataContext>(true);
				}
			}
		}
	}
}

#if WITH_EDITOR
void UAnimSequenceBase::RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces)
{
	Super::RemapTracksToNewSkeleton(NewSkeleton, bConvertSpaces);

	Controller->FindOrAddCurveNamesOnSkeleton(NewSkeleton, ERawCurveTrackTypes::RCT_Float);
	Controller->FindOrAddCurveNamesOnSkeleton(NewSkeleton, ERawCurveTrackTypes::RCT_Transform);
}
#endif

void UAnimSequenceBase::TickAssetPlayer(FAnimTickRecord& Instance, struct FAnimNotifyQueue& NotifyQueue, FAnimAssetTickContext& Context) const
{
	float CurrentTime = *(Instance.TimeAccumulator);
	if (Context.ShouldResyncToSyncGroup() && !Instance.BlendSpace.bIsEvaluator)	// HACK for 5.1.1 do allow us to fix UE-170739 without altering public API
	{
		// Synchronize the asset player time to the other sync group members when (re)joining the group
		CurrentTime = Context.GetAnimationPositionRatio() * GetPlayLength();
	}

	float PreviousTime = CurrentTime;
	float DeltaTime = 0.f;

	const float PlayRate = Instance.PlayRateMultiplier * this->RateScale;

	if( Context.IsLeader() )
	{
		DeltaTime = PlayRate * Context.GetDeltaTime();

		Context.SetLeaderDelta(DeltaTime);
		Context.SetPreviousAnimationPositionRatio(PreviousTime / GetPlayLength());

		if (DeltaTime != 0.f)
		{
			if (Instance.bCanUseMarkerSync && Context.CanUseMarkerPosition())
			{
				TickByMarkerAsLeader(*Instance.MarkerTickRecord, Context.MarkerTickContext, CurrentTime, PreviousTime, DeltaTime, Instance.bLooping, Instance.MirrorDataTable);
			}
			else
			{
				// Advance time
				FAnimationRuntime::AdvanceTime(Instance.bLooping, DeltaTime, CurrentTime, GetPlayLength());
				UE_LOG(LogAnimMarkerSync, Log, TEXT("Leader (%s) (normal advance)  - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping (%d) "), *GetName(), PreviousTime, CurrentTime, DeltaTime, Instance.bLooping ? 1 : 0);
			}
		}

		Context.SetAnimationPositionRatio(CurrentTime / GetPlayLength());
	}
	else
	{
		// Follow the leader
		if (Instance.bCanUseMarkerSync)
		{
			if (Context.CanUseMarkerPosition() && Context.MarkerTickContext.IsMarkerSyncStartValid())
			{
				TickByMarkerAsFollower(*Instance.MarkerTickRecord, Context.MarkerTickContext, CurrentTime, PreviousTime, Context.GetLeaderDelta(), Instance.bLooping, Instance.MirrorDataTable);
			}
			else
			{
				DeltaTime = PlayRate * Context.GetDeltaTime();
				// If leader is not valid, advance time as normal, do not jump position and pop.
				FAnimationRuntime::AdvanceTime(Instance.bLooping, DeltaTime, CurrentTime, GetPlayLength());
				UE_LOG(LogAnimMarkerSync, Log, TEXT("Follower (%s) (normal advance) - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping (%d) "), *GetName(), PreviousTime, CurrentTime, DeltaTime, Instance.bLooping ? 1 : 0);
			}
		}
		else
		{
			PreviousTime = Context.GetPreviousAnimationPositionRatio() * GetPlayLength();
			CurrentTime = Context.GetAnimationPositionRatio() * GetPlayLength();
			UE_LOG(LogAnimMarkerSync, Log, TEXT("Follower (%s) (normalized position advance) - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping (%d) "), *GetName(), PreviousTime, CurrentTime, DeltaTime, Instance.bLooping ? 1 : 0);
		}

		//@TODO: NOTIFIES: Calculate AdvanceType based on what the new delta time is

		if( CurrentTime != PreviousTime )
		{
			// Figure out delta time 
			DeltaTime = CurrentTime - PreviousTime;
			// if we went against play rate, then loop around.
			if( (DeltaTime * PlayRate) < 0.f )
			{
				DeltaTime += FMath::Sign<float>(PlayRate) * GetPlayLength();
			}
		}
	}

	// Assign the instance's TimeAccumulator after all side effects on CurrentTime have been applied
	*(Instance.TimeAccumulator) = CurrentTime;

	// Capture the final adjusted delta time and previous frame time as an asset player record
	check(Instance.DeltaTimeRecord);
	Instance.DeltaTimeRecord->Set(PreviousTime, DeltaTime);

	HandleAssetPlayerTickedInternal(Context, PreviousTime, DeltaTime, Instance, NotifyQueue);
}

void UAnimSequenceBase::TickByMarkerAsFollower(FMarkerTickRecord &Instance, FMarkerTickContext &MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping, const UMirrorDataTable* MirrorTable) const
{
	if (!Instance.IsValid(bLooping))
	{
		GetMarkerIndicesForPosition(MarkerContext.GetMarkerSyncStartPosition(), bLooping, Instance.PreviousMarker, Instance.NextMarker, CurrentTime, MirrorTable);
	}

	OutPreviousTime = CurrentTime;

	AdvanceMarkerPhaseAsFollower(MarkerContext, MoveDelta, bLooping, CurrentTime, Instance.PreviousMarker, Instance.NextMarker, MirrorTable);

	UE_LOG(LogAnimMarkerSync, Log, TEXT("Follower (%s) (TickByMarker) PreviousTime(%0.2f) CurrentTime(%0.2f) MoveDelta(%0.2f) Looping(%d) %s"), *GetName(), OutPreviousTime, CurrentTime, MoveDelta, bLooping ? 1 : 0, *MarkerContext.ToString());
}

void UAnimSequenceBase::TickByMarkerAsLeader(FMarkerTickRecord& Instance, FMarkerTickContext& MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping, const UMirrorDataTable* MirrorTable) const
{
	if (!Instance.IsValid(bLooping))
	{
		if (MarkerContext.IsMarkerSyncStartValid())
		{
			GetMarkerIndicesForPosition(MarkerContext.GetMarkerSyncStartPosition(), bLooping, Instance.PreviousMarker, Instance.NextMarker, CurrentTime, MirrorTable);
		}
		else
		{
			GetMarkerIndicesForTime(CurrentTime, bLooping, MarkerContext.GetValidMarkerNames(), Instance.PreviousMarker, Instance.NextMarker);
		}
		
	}

	MarkerContext.SetMarkerSyncStartPosition(GetMarkerSyncPositionFromMarkerIndicies(Instance.PreviousMarker.MarkerIndex, Instance.NextMarker.MarkerIndex, CurrentTime, MirrorTable));

	OutPreviousTime = CurrentTime;

	AdvanceMarkerPhaseAsLeader(bLooping, MoveDelta, MarkerContext.GetValidMarkerNames(), CurrentTime, Instance.PreviousMarker, Instance.NextMarker, MarkerContext.MarkersPassedThisTick, MirrorTable);

	MarkerContext.SetMarkerSyncEndPosition(GetMarkerSyncPositionFromMarkerIndicies(Instance.PreviousMarker.MarkerIndex, Instance.NextMarker.MarkerIndex, CurrentTime, MirrorTable));

	UE_LOG(LogAnimMarkerSync, Log, TEXT("Leader (%s) (TickByMarker) PreviousTime(%0.2f) CurrentTime(%0.2f) MoveDelta(%0.2f) Looping(%d) %s"), *GetName(), OutPreviousTime, CurrentTime, MoveDelta, bLooping ? 1 : 0, *MarkerContext.ToString());
}

bool CanNotifyUseTrack(const FAnimNotifyTrack& Track, const FAnimNotifyEvent& Notify)
{
	for (const FAnimNotifyEvent* Event : Track.Notifies)
	{
		if (FMath::IsNearlyEqual(Event->GetTime(), Notify.GetTime()))
		{
			return false;
		}
	}
	return true;
}

FAnimNotifyTrack& AddNewTrack(TArray<FAnimNotifyTrack>& Tracks)
{
	const int32 Index = Tracks.Add(FAnimNotifyTrack(*FString::FromInt(Tracks.Num() + 1), FLinearColor::White));
	return Tracks[Index];
}

void UAnimSequenceBase::RefreshCacheData()
{
	SortNotifies();

#if WITH_EDITOR
	for (int32 TrackIndex = 0; TrackIndex < AnimNotifyTracks.Num(); ++TrackIndex)
	{
		AnimNotifyTracks[TrackIndex].Notifies.Empty();
	}

	for (FAnimNotifyEvent& Notify : Notifies)
	{
		// Handle busted track indices
		if (!AnimNotifyTracks.IsValidIndex(Notify.TrackIndex))
		{
			// This really shouldn't happen (unless we are a cooked asset), but try to handle it
			ensureMsgf(GetOutermost()->bIsCookedForEditor, TEXT("AnimNotifyTrack: Anim (%s) has notify (%s) with track index (%i) that does not exist"), *GetFullName(), *Notify.NotifyName.ToString(), Notify.TrackIndex);

			// Don't create lots of extra tracks if we are way off supporting this track
			if (Notify.TrackIndex < 0 || Notify.TrackIndex > 20)
			{
				Notify.TrackIndex = 0;
			}

			while (!AnimNotifyTracks.IsValidIndex(Notify.TrackIndex))
			{
				AddNewTrack(AnimNotifyTracks);
			}
		}

		// Handle overlapping notifies
		FAnimNotifyTrack* TrackToUse = nullptr;
		int32 TrackIndexToUse = INDEX_NONE;
		for (int32 TrackOffset = 0; TrackOffset < AnimNotifyTracks.Num(); ++TrackOffset)
		{
			const int32 TrackIndex = (Notify.TrackIndex + TrackOffset) % AnimNotifyTracks.Num();
			if (CanNotifyUseTrack(AnimNotifyTracks[TrackIndex], Notify))
			{
				TrackToUse = &AnimNotifyTracks[TrackIndex];
				TrackIndexToUse = TrackIndex;
				break;
			}
		}

		if (TrackToUse == nullptr)
		{
			TrackToUse = &AddNewTrack(AnimNotifyTracks);
			TrackIndexToUse = AnimNotifyTracks.Num() - 1;
		}

		check(TrackToUse);
		check(TrackIndexToUse != INDEX_NONE);

		Notify.TrackIndex = TrackIndexToUse;
		TrackToUse->Notifies.Add(&Notify);
	}

	// this is a separate loop of checkin if they contains valid notifies
	for (int32 NotifyIndex = 0; NotifyIndex < Notifies.Num(); ++NotifyIndex)
	{
		const FAnimNotifyEvent& Notify = Notifies[NotifyIndex];
		// make sure if they can be placed in editor
		if (Notify.Notify)
		{
			if (Notify.Notify->CanBePlaced(this) == false)
			{
				static FName NAME_LoadErrors("LoadErrors");
				FMessageLog LoadErrors(NAME_LoadErrors);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
				Message->AddToken(FTextToken::Create(LOCTEXT("InvalidAnimNotify1", "The Animation ")));
				Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString(GetNameSafe(this))));
				Message->AddToken(FTextToken::Create(LOCTEXT("InvalidAnimNotify2", " contains invalid notify ")));
				Message->AddToken(FAssetNameToken::Create(Notify.Notify->GetPathName(), FText::FromString(GetNameSafe(Notify.Notify))));
				LoadErrors.Open();
			}
		}

		if (Notify.NotifyStateClass)
		{
			if (Notify.NotifyStateClass->CanBePlaced(this) == false)
			{
				static FName NAME_LoadErrors("LoadErrors");
				FMessageLog LoadErrors(NAME_LoadErrors);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
				Message->AddToken(FTextToken::Create(LOCTEXT("InvalidAnimNotify1", "The Animation ")));
				Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString(GetNameSafe(this))));
				Message->AddToken(FTextToken::Create(LOCTEXT("InvalidAnimNotify2", " contains invalid notify ")));
				Message->AddToken(FAssetNameToken::Create(Notify.NotifyStateClass->GetPathName(), FText::FromString(GetNameSafe(Notify.NotifyStateClass))));
				LoadErrors.Open();
			}
		}
	}
	// notification broadcast
	OnNotifyChanged.Broadcast();
#endif //WITH_EDITOR
}

int32 UAnimSequenceBase::GetNumberOfFrames() const
{
	return GetNumberOfSampledKeys();
}

int32 UAnimSequenceBase::GetNumberOfSampledKeys() const
{
	return GetSamplingFrameRate().AsFrameTime(GetPlayLength()).RoundToFrame().Value;
}

const FFrameRate& UAnimSequenceBase::GetSamplingFrameRate() const
{
	static const FFrameRate DefaultFrameRate = FFrameRate(DEFAULT_SAMPLERATE, 1);
	return DefaultFrameRate;
}

#if WITH_EDITOR
void UAnimSequenceBase::InitializeNotifyTrack()
{
	if ( AnimNotifyTracks.Num() == 0 ) 
	{
		AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("1"), FLinearColor::White ));
	}
}

int32 UAnimSequenceBase::GetFrameAtTime(const float Time) const
{
	return FMath::Clamp(GetSamplingFrameRate().AsFrameTime(Time).RoundToFrame().Value, 0, GetNumberOfSampledKeys() - 1);
}

float UAnimSequenceBase::GetTimeAtFrame(const int32 Frame) const
{
	return FMath::Clamp((float)GetSamplingFrameRate().AsSeconds(Frame), 0.f, GetPlayLength());
}

void UAnimSequenceBase::RegisterOnNotifyChanged(const FOnNotifyChanged& Delegate)
{
	OnNotifyChanged.Add(Delegate);
}
void UAnimSequenceBase::UnregisterOnNotifyChanged(void* Unregister)
{
	OnNotifyChanged.RemoveAll(Unregister);
}

void UAnimSequenceBase::ClampNotifiesAtEndOfSequence()
{
	const float NotifyClampTime = GetPlayLength();
	for(int i = 0; i < Notifies.Num(); ++ i)
	{
		if(Notifies[i].GetTime() >= GetPlayLength())
		{
			Notifies[i].SetTime(NotifyClampTime);
			Notifies[i].TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
		}
	}
}

EAnimEventTriggerOffsets::Type UAnimSequenceBase::CalculateOffsetForNotify(float NotifyDisplayTime) const
{
	if(NotifyDisplayTime == 0.f)
	{
		return EAnimEventTriggerOffsets::OffsetAfter;
	}
	else if(NotifyDisplayTime == GetPlayLength())
	{
		return EAnimEventTriggerOffsets::OffsetBefore;
	}
	return EAnimEventTriggerOffsets::NoOffset;
}

void UAnimSequenceBase::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	if(Notifies.Num() > 0)
	{
		FString NotifyList;

		// add notifies to 
		for(auto Iter=Notifies.CreateConstIterator(); Iter; ++Iter)
		{
			// only add if not BP anim notify since they're handled separate
			if(Iter->IsBlueprintNotify() == false)
			{
				NotifyList += FString::Printf(TEXT("%s%s"), *Iter->NotifyName.ToString(), *USkeleton::AnimNotifyTagDelimiter);
			}
		}
		
		if(NotifyList.Len() > 0)
		{
			OutTags.Add(FAssetRegistryTag(USkeleton::AnimNotifyTag, NotifyList, FAssetRegistryTag::TT_Hidden));
		}
	}

	// Add curve IDs to a tag list, or a blank tag if we have no curves.
	// The blank list is necessary when we attempt to delete a curve so
	// an old asset can be detected from its asset data so we load as few
	// as possible.
	FString CurveNameList;

	if (DataModel)
	{
		for(const FFloatCurve& Curve : DataModel->GetFloatCurves())
		{
			CurveNameList += FString::Printf(TEXT("%s%s"), *Curve.Name.DisplayName.ToString(), *USkeleton::CurveTagDelimiter);
		}
	}
	OutTags.Add(FAssetRegistryTag(USkeleton::CurveNameTag, CurveNameList, FAssetRegistryTag::TT_Hidden));
}

uint8* UAnimSequenceBase::FindNotifyPropertyData(int32 NotifyIndex, FArrayProperty*& ArrayProperty)
{
	// initialize to NULL
	ArrayProperty = NULL;

	if(Notifies.IsValidIndex(NotifyIndex))
	{
		return FindArrayProperty(TEXT("Notifies"), ArrayProperty, NotifyIndex);
	}
	return NULL;
}

uint8* UAnimSequenceBase::FindArrayProperty(const TCHAR* PropName, FArrayProperty*& ArrayProperty, int32 ArrayIndex)
{
	// find Notifies property start point
	FProperty* Property = FindFProperty<FProperty>(GetClass(), PropName);

	// found it and if it is array
	if (Property && Property->IsA(FArrayProperty::StaticClass()))
	{
		// find Property Value from UObject we got
		uint8* PropertyValue = Property->ContainerPtrToValuePtr<uint8>(this);

		// it is array, so now get ArrayHelper and find the raw ptr of the data
		ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
		FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyValue);

		if (ArrayProperty->Inner && ArrayIndex < ArrayHelper.Num())
		{
			//Get property data based on selected index
			return ArrayHelper.GetRawPtr(ArrayIndex);
		}
	}
	return NULL;
}

void UAnimSequenceBase::RefreshParentAssetData()
{
	Super::RefreshParentAssetData();

	check(ParentAsset);

	UAnimSequenceBase* ParentSeqBase = CastChecked<UAnimSequenceBase>(ParentAsset);

	RateScale = ParentSeqBase->RateScale;

	ValidateModel();
	Controller->OpenBracket(LOCTEXT("RefreshParentAssetData_Bracket", "Refreshing Parent Asset Data"));
	{
		const UAnimDataModel* ParentDataModel = ParentSeqBase->GetDataModel();

		if (!FMath::IsNearlyEqual(DataModel->GetPlayLength(), ParentDataModel->GetPlayLength()))
		{
			Controller->SetPlayLength(ParentDataModel->GetPlayLength());
		}
		
		Controller->RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float);
		for (const FFloatCurve& FloatCurve : ParentDataModel->GetFloatCurves())
		{
			const FAnimationCurveIdentifier CurveId(FloatCurve.Name, ERawCurveTrackTypes::RCT_Float);
			Controller->AddCurve(CurveId, FloatCurve.GetCurveTypeFlags());
			Controller->SetCurveKeys(CurveId, FloatCurve.FloatCurve.GetConstRefOfKeys());
		}

		Controller->RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform);
		for (const FTransformCurve& TransformCurve : ParentDataModel->GetTransformCurves())
		{
			const FAnimationCurveIdentifier CurveId(TransformCurve.Name, ERawCurveTrackTypes::RCT_Transform);
			Controller->AddCurve(CurveId, TransformCurve.GetCurveTypeFlags());

			TArray<FTransform> Values;
			TArray<float> Times;
			TransformCurve.GetKeys(Times, Values);

			Controller->SetTransformCurveKeys(CurveId, Values, Times);
		}
	}
	Controller->CloseBracket();

	// should do deep copy because notify contains outer
	Notifies = ParentSeqBase->Notifies;
	// update notify
	for (int32 NotifyIdx = 0; NotifyIdx < Notifies.Num(); ++NotifyIdx)
	{
		FAnimNotifyEvent& NotifyEvent = Notifies[NotifyIdx];
		if (NotifyEvent.Notify)
		{
			class UAnimNotify* NewNotifyClass = DuplicateObject(NotifyEvent.Notify, this);
			NotifyEvent.Notify = NewNotifyClass;
		}
		if (NotifyEvent.NotifyStateClass)
		{
			class UAnimNotifyState* NewNotifyStateClass = DuplicateObject(NotifyEvent.NotifyStateClass, this);
			NotifyEvent.NotifyStateClass = (NewNotifyStateClass);
		}

		NotifyEvent.Link(this, NotifyEvent.GetTime(), NotifyEvent.GetSlotIndex());
		NotifyEvent.EndLink.Link(this, NotifyEvent.GetTime() + NotifyEvent.Duration, NotifyEvent.GetSlotIndex());
	}
#if WITH_EDITORONLY_DATA
	// if you change Notifies array, this will need to be rebuilt
	AnimNotifyTracks = ParentSeqBase->AnimNotifyTracks;

	// fix up notify links, brute force and ugly code
	for (FAnimNotifyTrack& Track : AnimNotifyTracks)
	{
		for (FAnimNotifyEvent*& Notify : Track.Notifies)
		{
			for (int32 ParentNotifyIdx = 0; ParentNotifyIdx < ParentSeqBase->Notifies.Num(); ++ParentNotifyIdx)
			{
				if (Notify == &ParentSeqBase->Notifies[ParentNotifyIdx])
				{
					Notify = &Notifies[ParentNotifyIdx];
					break;
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

}
#endif	//WITH_EDITOR


/** Add curve data to Instance at the time of CurrentTime **/
void UAnimSequenceBase::EvaluateCurveData(FBlendedCurve& OutCurve, float CurrentTime, bool bForceUseRawData) const
{
	CSV_SCOPED_TIMING_STAT(Animation, EvaluateCurveData);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RawCurveData.EvaluateCurveData(OutCurve, CurrentTime);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

float UAnimSequenceBase::EvaluateCurveData(SmartName::UID_Type CurveUID, float CurrentTime, bool bForceUseRawData) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const FFloatCurve* Curve = (const FFloatCurve*)RawCurveData.GetCurveData(CurveUID, ERawCurveTrackTypes::RCT_Float);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	return Curve != nullptr ? Curve->Evaluate(CurrentTime) : 0.0f;
}

const FRawCurveTracks& UAnimSequenceBase::GetCurveData() const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RawCurveData;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool UAnimSequenceBase::HasCurveData(SmartName::UID_Type CurveUID, bool bForceUseRawData) const
{
#if WITH_EDITOR
	if (IsDataModelValid())
	{
		return DataModel->FindFloatCurve(FAnimationCurveIdentifier(CurveUID, ERawCurveTrackTypes::RCT_Float)) != nullptr;
	}
#endif
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return RawCurveData.GetCurveData(CurveUID) != nullptr;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UAnimSequenceBase::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	Super::Serialize(Ar);

	if (GetLinkerCustomVersion(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::AnimationDataModelInterface_BackedOut &&
		GetLinkerCustomVersion(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::BackoutAnimationDataModelInterface)
	{
		UE_LOG(LogAnimation, Fatal, TEXT("This package (%s) was saved with a version that had to be backed out and is no longer able to be loaded."), *GetPathNameSafe(this));
	}

	// fix up version issue and so on
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RawCurveData.PostSerialize(Ar);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UAnimSequenceBase::GetAnimationPose(struct FCompactPose& OutPose, FBlendedCurve & OutCurve, const FAnimExtractContext& ExtractionContext) const
{
	UE::Anim::FStackAttributeContainer TempAttributes;
	FAnimationPoseData OutAnimationPoseData(OutPose, OutCurve, TempAttributes);
	GetAnimationPose(OutAnimationPoseData, ExtractionContext);
}

void UAnimSequenceBase::HandleAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, struct FAnimNotifyQueue& NotifyQueue) const
{
	// Harvest and record notifies
	FAnimNotifyContext NotifyContext(Instance);
	GetAnimNotifies(PreviousTime, MoveDelta, NotifyContext);
	NotifyQueue.AddAnimNotifies(Context.ShouldGenerateNotifies(), NotifyContext.ActiveNotifies, Instance.EffectiveBlendWeight);
}

#if WITH_EDITOR
void UAnimSequenceBase::OnModelModified(const EAnimDataModelNotifyType& NotifyType, UAnimDataModel* Model, const FAnimDataModelNotifPayload& Payload)
{
	NotifyCollector.Handle(NotifyType);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InitializeNotifyTrack();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	auto HandleLengthChange = [&](float NewLength, float OldLength, float T0, float T1)
	{
		if (bPopulatingDataModel)
		{
			return;
		}
		
		const float StartRemoveTime = T0;
		const float EndRemoveTime = T1;

		// Total time value for frames that were removed
		const float Duration = T1 - T0;

		if (NewLength > OldLength)
		{
			const float InsertTime = T0;
			for (FAnimNotifyEvent& Notify : Notifies)
			{
				// Proportional notifies don't need to be adjusted
				if (Notify.GetLinkMethod() == EAnimLinkMethod::Proportional)
				{
					continue;
				}

				float CurrentTime = Notify.GetTime();
				float NewDuration = 0.f;

				// if state, make sure to adjust end time
				if (Notify.NotifyStateClass)
				{
					float NotifyDuration = Notify.GetDuration();
					float NotifyEnd = CurrentTime + NotifyDuration;
					if (NotifyEnd >= InsertTime)
					{
						NewDuration = NotifyDuration + Duration;
					}
					else
					{
						NewDuration = NotifyDuration;
					}
				}

				// Shift out notify by the time alloted for the inserted keys
				if (CurrentTime >= InsertTime)
				{
					CurrentTime += Duration;
				}

				// Clamps against the sequence length, ensuring that the notify does not end up outside of the playback bounds
				const float ClampedCurrentTime = FMath::Clamp(CurrentTime, 0.f, NewLength);

				Notify.Link(this, ClampedCurrentTime);
				Notify.SetDuration(NewDuration);

				if (ClampedCurrentTime == 0.f)
				{
					Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);
				}
				else if (ClampedCurrentTime == NewLength)
				{
					Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
				}
			}
		}
		else if (NewLength < OldLength)
		{
			// re-locate notifies
			for (FAnimNotifyEvent& Notify : Notifies)
			{
				// Proportional notifies don't need to be adjusted
				if (Notify.GetLinkMethod() == EAnimLinkMethod::Proportional)
				{
					continue;
				}

				float CurrentTime = Notify.GetTime();
				float NewDuration = 0.f;
				
				// if state, make sure to adjust end time
				if (Notify.NotifyStateClass)
				{
					float NotifyDuration = Notify.GetDuration();
					float NotifyEnd = CurrentTime + NotifyDuration;
					NewDuration = NotifyDuration;

					// If Notify is inside of the trimmed time frame, zero out the duration
					if (CurrentTime >= StartRemoveTime && NotifyEnd <= EndRemoveTime)
					{
						// small number @todo see if there is define for this
						NewDuration = DataModel->GetFrameRate().AsInterval();
					}
					// If Notify overlaps trimmed time frame, remove trimmed duration
					else if (CurrentTime < EndRemoveTime && NotifyEnd > EndRemoveTime)
					{
						NewDuration = NotifyEnd - Duration - CurrentTime;
					}

					NewDuration = FMath::Max(NewDuration, (float)DataModel->GetFrameRate().AsInterval());
				}

				if (CurrentTime >= StartRemoveTime && CurrentTime <= EndRemoveTime)
				{
					CurrentTime = StartRemoveTime;
				}
				else if (CurrentTime > EndRemoveTime)
				{
					CurrentTime -= Duration;
				}

				const float ClampedCurrentTime = FMath::Clamp(CurrentTime, 0.f, NewLength);

				Notify.Link(this, ClampedCurrentTime);
				Notify.SetDuration(NewDuration);

				if (ClampedCurrentTime == 0.f)
				{
					Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetAfter);
				}
				else if (ClampedCurrentTime == NewLength)
				{
					Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
				}
			}
		}	
	};

	// Source animation curve data can be coarse, so copy the data down and remove any redundant keys
	auto CopyCurvesFromModel = [this]()
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		RawCurveData.FloatCurves = DataModel->GetFloatCurves();
		RawCurveData.TransformCurves = DataModel->GetTransformCurves();
		RawCurveData.RemoveRedundantKeys();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	};

	switch (NotifyType)
	{
		case EAnimDataModelNotifyType::SequenceLengthChanged:
		{
			const FSequenceLengthChangedPayload& TypedPayload = Payload.GetPayload<FSequenceLengthChangedPayload>();
			const float PreviousSequenceLength = TypedPayload.PreviousLength;
			const float CurrentSequenceLength = Model->GetPlayLength();

			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			SequenceLength = CurrentSequenceLength;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			HandleLengthChange(CurrentSequenceLength, PreviousSequenceLength, TypedPayload.T0, TypedPayload.T1);

			// Ensure that we only clamp the notifies at the end of a bracket
			if (NotifyCollector.IsNotWithinBracket())
			{
				ClampNotifiesAtEndOfSequence();
				CopyCurvesFromModel();
			}
			
			break;
		}

		case EAnimDataModelNotifyType::CurveAdded:
		case EAnimDataModelNotifyType::CurveChanged:
		case EAnimDataModelNotifyType::CurveRemoved:
		case EAnimDataModelNotifyType::CurveFlagsChanged:
		case EAnimDataModelNotifyType::CurveScaled:
		{
			if (NotifyCollector.IsNotWithinBracket())
			{
				CopyCurvesFromModel();
			}
			break;
		}
		
		case EAnimDataModelNotifyType::CurveRenamed:
		{
			const FCurveRenamedPayload& TypedPayload = Payload.GetPayload<FCurveRenamedPayload>();
			FAnimCurveBase* CurvePtr = [this, Identifier=TypedPayload.Identifier]() -> FAnimCurveBase*
			{
				if (Identifier.CurveType == ERawCurveTrackTypes::RCT_Float)
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					return RawCurveData.FloatCurves.FindByPredicate([SmartName=Identifier.InternalName](FFloatCurve& Curve)
	                {
	                    return Curve.Name == SmartName;
	                });
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
				}
				else if (Identifier.CurveType == ERawCurveTrackTypes::RCT_Transform)
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					return RawCurveData.TransformCurves.FindByPredicate([SmartName=Identifier.InternalName](FTransformCurve& Curve)
                    {
                        return Curve.Name == SmartName;
                    });
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
				}

				return nullptr;
			}();
				
			if (CurvePtr)
			{
				CurvePtr->Name = TypedPayload.NewIdentifier.InternalName;
			}
			break;
		}

		case EAnimDataModelNotifyType::Populated:
		{
			const float CurrentSequenceLength = Model->GetPlayLength();
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			SequenceLength = CurrentSequenceLength;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			if (NotifyCollector.IsNotWithinBracket())
			{
				ClampNotifiesAtEndOfSequence();
				CopyCurvesFromModel();
			}

			break;
		}

		case EAnimDataModelNotifyType::BracketClosed:
		{
			if (NotifyCollector.IsNotWithinBracket())
			{
				const auto CurveCopyNotifies = { EAnimDataModelNotifyType::CurveAdded, EAnimDataModelNotifyType::CurveChanged, EAnimDataModelNotifyType::CurveRemoved, EAnimDataModelNotifyType::CurveFlagsChanged, EAnimDataModelNotifyType::CurveScaled, EAnimDataModelNotifyType::Populated, EAnimDataModelNotifyType::Reset };

				const auto LengthChangingNotifies = { EAnimDataModelNotifyType::SequenceLengthChanged, EAnimDataModelNotifyType::FrameRateChanged, EAnimDataModelNotifyType::Reset, EAnimDataModelNotifyType::Populated };

				if (NotifyCollector.Contains(CurveCopyNotifies) || NotifyCollector.Contains(LengthChangingNotifies))
				{
					CopyCurvesFromModel();
				}
				
				if (NotifyCollector.Contains(LengthChangingNotifies))
				{
					CopyCurvesFromModel();
					ClampNotifiesAtEndOfSequence();
				}
			}
			break;
		}
	}
}

UAnimDataModel* UAnimSequenceBase::GetDataModel() const
{
	return DataModel;
}

IAnimationDataController& UAnimSequenceBase::GetController()
{
	ValidateModel();

	if(Controller == nullptr)
	{
		IAnimationDataControllerModule& ControllerModule = FModuleManager::Get().GetModuleChecked<IAnimationDataControllerModule>("AnimationDataController");
		Controller = ControllerModule.GetController();
		Controller->SetModel(DataModel);
	}

	ensureAlways(Controller->GetModel() == DataModel);

	return *Controller;
}

void UAnimSequenceBase::PopulateModel()
{
	check(!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject));	
	
	// Make a copy of the current data, to mitigate any changes due to notify callbacks
	const FFrameRate FrameRate = GetSamplingFrameRate();
	const float PlayLength = GetPlayLength();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const FRawCurveTracks CurveData = RawCurveData;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
		
	IAnimationDataController::FScopedBracket ScopedBracket(Controller, LOCTEXT("UAnimSequenceBase::PopulateModel_Bracket", "Generating Animation Model Data for Animation Sequence Base"));
	Controller->SetPlayLength(PlayLength);
	Controller->SetFrameRate(FrameRate);

	USkeleton* TargetSkeleton = GetSkeleton();	
	UE::Anim::CopyCurveDataToModel(CurveData, TargetSkeleton,  *Controller);	
}

void UAnimSequenceBase::BindToModelModificationEvent()
{
	ValidateModel();
	DataModel->GetModifiedEvent().RemoveAll(this);
	DataModel->GetModifiedEvent().AddUObject(this, &UAnimSequenceBase::OnModelModified);
}

void UAnimSequenceBase::CopyDataModel(const UAnimDataModel* ModelToDuplicate)
{
	checkf(ModelToDuplicate != nullptr, TEXT("Invalidate data model %s"), *GetFullName());
	if (DataModel)
	{
		DataModel->GetModifiedEvent().RemoveAll(this);
	}

	DataModel = DuplicateObject(ModelToDuplicate, this);
	Controller->SetModel(DataModel);

	BindToModelModificationEvent();
}

void UAnimSequenceBase::CreateModel()
{
	checkf(DataModel == nullptr, TEXT("Invalid attempt to override the existing data model %s"), *GetFullName());
	DataModel = NewObject<UAnimDataModel>(this, FName(TEXT("AnimationDataModel")));
		
	BindToModelModificationEvent();
}

void UAnimSequenceBase::ValidateModel() const
{
	checkf(DataModel != nullptr, TEXT("Invalid AnimSequenceBase state (%s), no data model found"), *GetPathName());
}

bool UAnimSequenceBase::ShouldDataModelBeValid() const
{
	return
#if WITH_EDITOR
		!GetOutermost()->HasAnyPackageFlags(PKG_Cooked);
#else
		false;
#endif
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE 

