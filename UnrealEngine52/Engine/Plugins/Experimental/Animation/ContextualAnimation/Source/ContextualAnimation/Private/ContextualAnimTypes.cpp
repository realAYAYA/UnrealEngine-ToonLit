// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimTypes.h"
#include "AnimationUtils.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AnimNotifyState_MotionWarping.h"
#include "ContextualAnimUtilities.h"
#include "RootMotionModifier.h"
#include "ContextualAnimSelectionCriterion.h"
#include "ContextualAnimSceneActorComponent.h"
#include "ContextualAnimSceneAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContextualAnimTypes)

DEFINE_LOG_CATEGORY(LogContextualAnim);

const FContextualAnimSceneBinding FContextualAnimSceneBinding::InvalidBinding;
const FContextualAnimTrack FContextualAnimTrack::EmptyTrack;
const FContextualAnimIKTarget FContextualAnimIKTarget::InvalidIKTarget;
const FContextualAnimIKTargetDefContainer FContextualAnimIKTargetDefContainer::EmptyContainer;
const FContextualAnimRoleDefinition FContextualAnimRoleDefinition::InvalidRoleDefinition;

// FContextualAnimAlignmentTrackContainer
///////////////////////////////////////////////////////////////////////

FTransform FContextualAnimAlignmentTrackContainer::ExtractTransformAtTime(const FName& TrackName, float Time) const
{
	const int32 TrackIndex = Tracks.TrackNames.IndexOfByKey(TrackName);
	return ExtractTransformAtTime(TrackIndex, Time);
}

FTransform FContextualAnimAlignmentTrackContainer::ExtractTransformAtTime(int32 TrackIndex, float Time) const
{
	FTransform AlignmentTransform = FTransform::Identity;

	if (Tracks.AnimationTracks.IsValidIndex(TrackIndex))
	{
		const FRawAnimSequenceTrack& Track = Tracks.AnimationTracks[TrackIndex];
		const int32 TotalFrames = Track.PosKeys.Num();
		const float TrackLength = (TotalFrames - 1) * SampleInterval;
		FAnimationUtils::ExtractTransformFromTrack(Track, Time, TotalFrames, TrackLength, EAnimInterpolationType::Linear, AlignmentTransform);
	}

	return AlignmentTransform;
}

float FContextualAnimTrack::GetSyncTimeForWarpSection(int32 WarpSectionIndex) const
{
	float StartTime, EndTime;
	GetStartAndEndTimeForWarpSection(WarpSectionIndex, StartTime, EndTime);
	return EndTime;
}

float FContextualAnimTrack::GetSyncTimeForWarpSection(const FName& WarpSectionName) const
{
	float StartTime, EndTime;
	GetStartAndEndTimeForWarpSection(WarpSectionName, StartTime, EndTime);
	return EndTime;
}

void FContextualAnimTrack::GetStartAndEndTimeForWarpSection(int32 WarpSectionIndex, float& OutStartTime, float& OutEndTime) const
{
	//@TODO: We need a better way to identify warping sections withing the animation. This is just a temp solution
	//@TODO: We should cache this data

	OutStartTime = 0.f;
	OutEndTime = 0.f;

	if (Animation && WarpSectionIndex >= 0)
	{
		FName LastWarpTargetName = NAME_None;
		int32 LastWarpSectionIndex = INDEX_NONE;

		for (int32 Idx = 0; Idx < Animation->Notifies.Num(); Idx++)
		{
			const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Idx];
			if (const UAnimNotifyState_MotionWarping* Notify = Cast<const UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass))
			{
				if (const URootMotionModifier_Warp* Modifier = Cast<const URootMotionModifier_Warp>(Notify->RootMotionModifier))
				{
					const FName WarpTargetName = Modifier->WarpTargetName;
					if (WarpTargetName != NAME_None)
					{
						// First valid warping window. Initialize everything
						if (LastWarpSectionIndex == INDEX_NONE)
						{
							LastWarpTargetName = WarpTargetName;
							OutStartTime = NotifyEvent.GetTriggerTime();
							OutEndTime = NotifyEvent.GetEndTriggerTime();
							LastWarpSectionIndex = 0;
						}
						// If we hit another warping window but the sync point is the same as the previous, update SyncTime.
						// This is to deal with cases where a first short window is used to face the alignment point and a second one to perform the rest of the warp
						else if (WarpTargetName == LastWarpTargetName)
						{
							OutStartTime = NotifyEvent.GetTriggerTime();
							OutEndTime = NotifyEvent.GetEndTriggerTime();
						}
						// If we hit another warping window but with a different sync point name means that we have hit the first window of another warping section
						else
						{
							// If we haven't reached the desired WarpSection yet. Update control vars and keep moving
							if (WarpSectionIndex > LastWarpSectionIndex)
							{
								LastWarpTargetName = WarpTargetName;
								OutStartTime = NotifyEvent.GetTriggerTime();
								OutEndTime = NotifyEvent.GetEndTriggerTime();
								LastWarpSectionIndex++;
							}
							// Otherwise, stop here and return the value of the last window we found
							else
							{
								break;
							}
						}
					}
				}
			}
		}
	}
}

void FContextualAnimTrack::GetStartAndEndTimeForWarpSection(const FName& WarpSectionName, float& OutStartTime, float& OutEndTime) const
{
	//@TODO: We need a better way to identify warping sections within the animation. This is just a temp solution
	//@TODO: We should cache this data

	OutStartTime = 0.f;
	OutEndTime = 0.f;

	int32 Index = INDEX_NONE;
	float LastEndTime = 0.f;
	if (Animation && WarpSectionName != NAME_None)
	{
		for (int32 Idx = 0; Idx < Animation->Notifies.Num(); Idx++)
		{
			const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Idx];
			if (const UAnimNotifyState_MotionWarping* Notify = Cast<const UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass))
			{
				if (const URootMotionModifier_Warp* Config = Cast<const URootMotionModifier_Warp>(Notify->RootMotionModifier))
				{
					const FName WarpTargetName = Config->WarpTargetName;
					if (WarpSectionName == WarpTargetName)
					{
						const float NotifyEndTriggerTime = NotifyEvent.GetEndTriggerTime();
						if(NotifyEndTriggerTime > LastEndTime)
						{
							LastEndTime = NotifyEndTriggerTime;
							Index = Idx;
						}
					}
				}
			}
		}
	}

	if(Index != INDEX_NONE)
	{
		const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Index];
		OutStartTime = NotifyEvent.GetTriggerTime();
		OutEndTime = NotifyEvent.GetEndTriggerTime();
	}
}

float FContextualAnimTrack::FindBestAnimStartTime(const FVector& LocalLocation) const
{
	float BestTime = 0.f;

	if (AnimMaxStartTime < 0.f)
	{
		return BestTime;
	}

	const FVector SyncPointLocation = GetAlignmentTransformAtSyncTime().GetLocation();
	const float PerfectDistToSyncPointSq = GetAlignmentTransformAtEntryTime().GetTranslation().SizeSquared2D();
	const float ActualDistToSyncPointSq = FVector::DistSquared2D(LocalLocation, SyncPointLocation);

	if (ActualDistToSyncPointSq < PerfectDistToSyncPointSq)
	{
		float BestDistance = MAX_FLT;
		TArrayView<const FVector3f> PosKeys(AlignmentData.Tracks.AnimationTracks[0].PosKeys.GetData(), AlignmentData.Tracks.AnimationTracks[0].PosKeys.Num());

		//@TODO: Very simple search for now. Replace with Distance Matching + Pose Matching
		for (int32 Idx = 0; Idx < PosKeys.Num(); Idx++)
		{
			const float Time = Idx * AlignmentData.SampleInterval;
			if (AnimMaxStartTime > 0.f && Time >= AnimMaxStartTime)
			{
				break;
			}

			const float DistFromCurrentFrameToSyncPointSq = FVector::DistSquared2D(SyncPointLocation, (FVector)PosKeys[Idx]);
			if (DistFromCurrentFrameToSyncPointSq < ActualDistToSyncPointSq)
			{
				BestTime = Time;
				break;
			}
		}
	}

	return BestTime;
}

bool FContextualAnimTrack::DoesQuerierPassSelectionCriteria(const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier) const
{
	for (const UContextualAnimSelectionCriterion* Criterion : SelectionCriteria)
	{
		if (Criterion && !Criterion->DoesQuerierPassCondition(Primary, Querier))
		{
			return false;
		}
	}

	return true;
}

FTransform FContextualAnimTrack::GetRootTransformAtTime(float Time) const
{
	FTransform RootTransform = FTransform::Identity;
	if (Animation)
	{
		RootTransform = UContextualAnimUtilities::ExtractRootTransformFromAnimation(Animation, Time);
	}

	return RootTransform * MeshToScene;
}

// FContextualAnimSceneBindingContext
///////////////////////////////////////////////////////////////////////

void FContextualAnimSceneBindingContext::SetExternalTransform(const FTransform& InTransform)
{
	ExternalTransform = InTransform;
}

FTransform FContextualAnimSceneBindingContext::GetTransform() const
{
	if (ExternalTransform.IsSet())
	{
		return ExternalTransform.GetValue();
	}
	else if (AActor* ActorPtr = GetActor())
	{
		return ActorPtr->GetActorTransform();
	}

	return FTransform::Identity;
}

FVector FContextualAnimSceneBindingContext::GetVelocity() const
{
	if (ExternalVelocity.IsSet())
	{
		return ExternalVelocity.GetValue();
	}
	else if (AActor* ActorPtr = GetActor())
	{
		return ActorPtr->GetVelocity();
	}

	return FVector::ZeroVector;
}

bool FContextualAnimSceneBindingContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	//@TODO: Serialize optional properties
	Ar << Actor;
	bOutSuccess = true;
	return true;
}

// FContextualAnimSceneBinding
///////////////////////////////////////////////////////////////////////

FContextualAnimSceneBinding::FContextualAnimSceneBinding(const FContextualAnimSceneBindingContext& InContext, const FContextualAnimTrack& InAnimTrack)
{
	Context = InContext;
	AnimTrackIdx = InAnimTrack.AnimTrackIdx;
}

void FContextualAnimSceneBinding::SetAnimTrack(const FContextualAnimTrack& InAnimTrack) 
{
	AnimTrackIdx = InAnimTrack.AnimTrackIdx;
}

UContextualAnimSceneActorComponent* FContextualAnimSceneBinding::GetSceneActorComponent() const
{
	if (CachedSceneActorComp == nullptr)
	{
		if (AActor* Actor = Context.GetActor())
		{
			CachedSceneActorComp = Actor->FindComponentByClass<UContextualAnimSceneActorComponent>();
		}
	}

	return CachedSceneActorComp;
}

UAnimInstance* FContextualAnimSceneBinding::GetAnimInstance() const
{
	if (CachedAnimInstance == nullptr)
	{
		CachedAnimInstance = UContextualAnimUtilities::TryGetAnimInstance(GetActor());
	}

	return CachedAnimInstance;
}

USkeletalMeshComponent* FContextualAnimSceneBinding::GetSkeletalMeshComponent() const
{
	if(CachedSkeletalMesh == nullptr)
	{
		CachedSkeletalMesh = UContextualAnimUtilities::TryGetSkeletalMeshComponent(GetActor());
	}

	return CachedSkeletalMesh;
}

FAnimMontageInstance* FContextualAnimSceneBinding::GetAnimMontageInstance() const
{
	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		return AnimInstance->GetActiveMontageInstance();
	}

	return nullptr;
}

float FContextualAnimSceneBinding::GetAnimMontageTime() const
{
	const FAnimMontageInstance* MontageInstance = GetAnimMontageInstance();
	return MontageInstance ? MontageInstance->GetPosition() : -1.f;
}

FName FContextualAnimSceneBinding::GetCurrentSection() const
{
	const FAnimMontageInstance* MontageInstance = GetAnimMontageInstance();
	return MontageInstance ? MontageInstance->GetCurrentSection() : NAME_None;
}

int32 FContextualAnimSceneBinding::GetCurrentSectionIndex() const
{
	if (const FAnimMontageInstance* MontageInstance = GetAnimMontageInstance())
	{
		float CurrentPosition;
		return MontageInstance->Montage->GetAnimCompositeSectionIndexFromPos(MontageInstance->GetPosition(), CurrentPosition);
	}

	return INDEX_NONE;
}

bool FContextualAnimSceneBinding::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Context.NetSerialize(Ar, Map, bOutSuccess);
	Ar << AnimTrackIdx;
	bOutSuccess = true;
	return true;
}

// FContextualAnimSceneBindings
///////////////////////////////////////////////////////////////////////

FContextualAnimSceneBindings::FContextualAnimSceneBindings(const UContextualAnimSceneAsset& InSceneAsset, int32 InSectionIdx, int32 InAnimSetIdx)
{
	check(InSceneAsset.HasValidData());
	SceneAsset = &InSceneAsset;
	SectionIdx = InSectionIdx;
	AnimSetIdx = InAnimSetIdx;
	GenerateUniqueId();
}

void FContextualAnimSceneBindings::GenerateUniqueId()
{
	static uint8 IncrementID = 0;
	IncrementID = IncrementID < UINT8_MAX ? IncrementID + 1 : 0;
	Id = IncrementID;
}

bool FContextualAnimSceneBindings::IsValid() const
{
	return Id != 0 && SceneAsset.IsValid() && SceneAsset->HasValidData() && Num() > 0;
}

void FContextualAnimSceneBindings::Reset()
{
	Id = 0;
	SceneAsset.Reset();
	SectionIdx = INDEX_NONE;
	AnimSetIdx = INDEX_NONE;
	Data.Reset();
	SceneInstancePtr.Reset();
}

void FContextualAnimSceneBindings::Clear()
{
	Data.Reset();
	SceneInstancePtr.Reset();
}

const FContextualAnimSceneBinding* FContextualAnimSceneBindings::GetSyncLeader() const
{
	//@TODO: Return first secondary binding as sync leader for now. This may have to be explicitly defined, either in the SceneAsset or when creating the bindings.
	return Data.FindByPredicate([this](const FContextualAnimSceneBinding& Item) { return GetRoleFromBinding(Item) != SceneAsset->GetPrimaryRole(); });
}

bool FContextualAnimSceneBindings::BindActorToRole(AActor& ActorRef, FName Role)
{
	const UContextualAnimSceneActorComponent* Comp = ActorRef.FindComponentByClass<UContextualAnimSceneActorComponent>();
	if (Comp == nullptr)
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("FContextualAnimSceneBindings::BindActorToRole. Failed to bind Actor: '%s' to Role: '%s'. Reason: Missing SceneActorComp"),
			*GetNameSafe(&ActorRef), *Role.ToString());
		return false;
	}

	if(const FContextualAnimSceneBinding* Binding = FindBindingByRole(Role))
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("FContextualAnimSceneBindings::BindActorToRole. Failed to bind Actor: '%s' to Role: '%s'. Reason: %s already bound"), 
			*GetNameSafe(&ActorRef), *Role.ToString(), *GetNameSafe(Binding->GetActor()));
		return false;
	}

	const FContextualAnimTrack* AnimTrackPtr = SceneAsset->GetAnimTrack(SectionIdx, AnimSetIdx, Role);
	if(AnimTrackPtr == nullptr)
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("FContextualAnimSceneBindings::BindActorToRole. Failed to bind Actor: '%s' to Role: '%s'. Reason: Can't find valid AnimTrack for it. SceneAsset: %s SectionIdx: %d AnimSetIdx: %d"),
			*GetNameSafe(&ActorRef), *Role.ToString(), *GetNameSafe(SceneAsset.Get()), SectionIdx, AnimSetIdx);
		return false;
	}

	Data.Add(FContextualAnimSceneBinding(FContextualAnimSceneBindingContext(&ActorRef), *AnimTrackPtr));

	return true;
}

const FContextualAnimTrack& FContextualAnimSceneBindings::GetAnimTrackFromBinding(const FContextualAnimSceneBinding& Binding) const
{
	if (const FContextualAnimTrack* AnimTrack = GetSceneAsset()->GetAnimTrack(SectionIdx, AnimSetIdx, Binding.AnimTrackIdx))
	{
		return *AnimTrack;
	}

	return FContextualAnimTrack::EmptyTrack;
}

const FContextualAnimIKTargetDefContainer& FContextualAnimSceneBindings::GetIKTargetDefContainerFromBinding(const FContextualAnimSceneBinding& Binding) const
{
	return GetSceneAsset()->GetIKTargetDefsForRoleInSection(GetSectionIdx(), GetRoleFromBinding(Binding));
}

FTransform FContextualAnimSceneBindings::GetIKTargetTransformFromBinding(const FContextualAnimSceneBinding& Binding, const FName& TrackName, float Time) const
{
	return GetSceneAsset()->GetIKTargetTransform(SectionIdx, AnimSetIdx, Binding.AnimTrackIdx, TrackName, Time);
}

FTransform FContextualAnimSceneBindings::GetAlignmentTransformFromBinding(const FContextualAnimSceneBinding& Binding, const FName& TrackName, float Time) const
{
	return GetSceneAsset()->GetAlignmentTransform(SectionIdx, AnimSetIdx, Binding.AnimTrackIdx, TrackName, Time);
}

const FName& FContextualAnimSceneBindings::GetRoleFromBinding(const FContextualAnimSceneBinding& Binding) const
{
	return GetAnimTrackFromBinding(Binding).Role;
}

bool FContextualAnimSceneBindings::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << Id;
	Ar << SceneAsset;
	Ar << SectionIdx;
	Ar << AnimSetIdx;
	bOutSuccess = SafeNetSerializeTArray_WithNetSerialize<10>(Ar, Data, Map);
	return true;
}

bool FContextualAnimSceneBindings::CheckConditions(const UContextualAnimSceneAsset& SceneAsset, int32 SectionIdx, int32 AnimSetIdx, const TMap<FName, FContextualAnimSceneBindingContext>& Params)
{
	check(SceneAsset.HasValidData());

	auto DoCheck = [&SceneAsset, SectionIdx, AnimSetIdx](const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier, const FName& Role)
	{
		if (AActor* Actor = Querier.GetActor())
		{
			UContextualAnimSceneActorComponent* SceneActorComp = Actor->FindComponentByClass<UContextualAnimSceneActorComponent>();
			if (SceneActorComp == nullptr)
			{
				UE_LOG(LogContextualAnim, Verbose, TEXT("FContextualAnimSceneBindings::CheckConditions Failed. Reason: Missing ContextualAnimSceneActorComp. SceneAsset: %s Actor: %s Role: %s SectionIdx: %d AnimSetIdx: %d"),
					*GetNameSafe(&SceneAsset), *GetNameSafe(Actor), *Role.ToString(), SectionIdx, AnimSetIdx);

				return false;
			}
		}

		const FContextualAnimTrack* AnimTrack = SceneAsset.GetAnimTrack(SectionIdx, AnimSetIdx, Role);
		if (AnimTrack == nullptr || AnimTrack->DoesQuerierPassSelectionCriteria(Primary, Querier) == false)
		{
			UE_LOG(LogContextualAnim, Verbose, TEXT("FContextualAnimSceneBindings::CheckConditions Failed. Reason: Reason: Can't find valid track for actor. SceneAsset: %s Actor: %s Role: %s SectionIdx: %d AnimSetIdx: %d"),
				*GetNameSafe(&SceneAsset), *GetNameSafe(Querier.GetActor()), *Role.ToString(), SectionIdx, AnimSetIdx);

			return false;
		}

		return true;
	};

	// Find the actor that should be bound to the primary Role.
	const FName PrimaryRole = SceneAsset.GetPrimaryRole();
	const FContextualAnimSceneBindingContext* PrimaryPtr = Params.Find(PrimaryRole);
	if (PrimaryPtr == nullptr)
	{
		UE_LOG(LogContextualAnim, Verbose, TEXT("FContextualAnimSceneBindings::CheckConditions Failed. Reason: Can't find valid actor for primary role. SceneAsset: %s PrimaryRole: %s"),
			*GetNameSafe(&SceneAsset), *PrimaryRole.ToString());

		return false;
	}

	// Test primary actor first
	// Passing the same data twice (as primary and querier) feels weird, but this allow us to run the selection mechanism even on the primary actor.
	if (DoCheck(*PrimaryPtr, *PrimaryPtr, PrimaryRole) == false)
	{
		return false;
	}

	// Now test secondary actors
	int32 ValidEntries = 1;
	for (const auto& Pair : Params)
	{
		FName RoleToBind = Pair.Key;
		if (RoleToBind != PrimaryRole)
		{
			if (DoCheck(*PrimaryPtr, Pair.Value, RoleToBind) == false)
			{
				return false;
			}

			ValidEntries++;
		}
	}

	const int32 NumMandatoryRoles = SceneAsset.GetNumMandatoryRoles(SectionIdx, AnimSetIdx);
	if (ValidEntries < NumMandatoryRoles)
	{
		return false;
	}

	return true;
}

bool FContextualAnimSceneBindings::TryCreateBindings(const UContextualAnimSceneAsset& SceneAsset, int32 SectionIdx, int32 AnimSetIdx, const TMap<FName, FContextualAnimSceneBindingContext>& Params, FContextualAnimSceneBindings& OutBindings)
{
	check(SceneAsset.HasValidData());
	
	OutBindings.Reset();

	if (FContextualAnimSceneBindings::CheckConditions(SceneAsset, SectionIdx, AnimSetIdx, Params))
	{
		OutBindings = FContextualAnimSceneBindings(SceneAsset, SectionIdx, AnimSetIdx);
		for (const auto& Pair : Params)
		{
			const FContextualAnimTrack* AnimTrackPtr = SceneAsset.GetAnimTrack(SectionIdx, AnimSetIdx, Pair.Key);
			check(AnimTrackPtr);

			OutBindings.Data.Add(FContextualAnimSceneBinding(Pair.Value, *AnimTrackPtr));
		}

		check(OutBindings.IsValid());
		return true;
	}

	return false;
}

bool FContextualAnimSceneBindings::TryCreateBindings(const UContextualAnimSceneAsset& SceneAsset, int32 SectionIdx, int32 AnimSetIdx, const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Secondary, FContextualAnimSceneBindings& OutBindings)
{
	check(SceneAsset.HasValidData());

	OutBindings.Reset();

	const TArray<FContextualAnimRoleDefinition>& Roles = SceneAsset.GetRolesAsset()->Roles;
	if (Roles.Num() > 2)
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("FContextualAnimSceneBindings::TryCreateBindings Failed. Reason: Trying to create bindings with two actors for a SceneAsset with more than two roles. SceneAsset: %s Num Roles: %d SectionIdx: %d AnimSetIdx: %d"),
			*GetNameSafe(&SceneAsset), Roles.Num(), SectionIdx, AnimSetIdx);

		return false;
	}

	TMap<FName, FContextualAnimSceneBindingContext> Params;
	for(const FContextualAnimRoleDefinition& RoleDef : Roles)
	{
		const FName PrimaryRole = SceneAsset.GetPrimaryRole();
		Params.Add(RoleDef.Name, (RoleDef.Name == PrimaryRole) ? Primary : Secondary);
	}

	return TryCreateBindings(SceneAsset, SectionIdx, AnimSetIdx, Params, OutBindings);
}

bool FContextualAnimSceneBindings::TryCreateBindings(const UContextualAnimSceneAsset& SceneAsset, int32 SectionIdx, const TMap<FName, FContextualAnimSceneBindingContext>& Params, FContextualAnimSceneBindings& OutBindings)
{
	check(SceneAsset.HasValidData());
	OutBindings.Reset();

	int32 AnimSetIdxSelected = INDEX_NONE;

	const int32 NumSets = SceneAsset.GetNumAnimSetsInSection(SectionIdx);
	if (NumSets == 1)
	{
		if (CheckConditions(SceneAsset, SectionIdx, 0, Params))
		{
			AnimSetIdxSelected = 0;
		}
	}
	else if (NumSets > 1)
	{
		const FContextualAnimSceneSection* Section = SceneAsset.GetSection(SectionIdx);
		TArray<TTuple<int32, float>, TInlineAllocator<5>> ValidSets; // 0: AnimSetIdx, 1: AnimSetRandomWeight
		
		float TotalWeight = 0;
		for (int32 AnimSetIdx = 0; AnimSetIdx < NumSets; AnimSetIdx++)
		{
			if (CheckConditions(SceneAsset, SectionIdx, AnimSetIdx, Params))
			{
				const float AnimSetRandomWeight = FMath::Max(Section->GetAnimSet(AnimSetIdx)->RandomWeight, 0);
				ValidSets.Add(MakeTuple(AnimSetIdx, AnimSetRandomWeight));
				TotalWeight += AnimSetRandomWeight;
			}
		}

		float RandomValue = FMath::RandRange(0.f, TotalWeight);
		for (int32 Idx = 0; Idx < ValidSets.Num(); Idx++)
		{
			RandomValue -= FMath::Max(ValidSets[Idx].Get<1>(), 0);
			if (RandomValue <= 0)
			{
				AnimSetIdxSelected = ValidSets[Idx].Get<0>();
				break;
			}
		}
	}

	if (AnimSetIdxSelected != INDEX_NONE)
	{
		OutBindings = FContextualAnimSceneBindings(SceneAsset, SectionIdx, AnimSetIdxSelected);
		for (const auto& Pair : Params)
		{
			const FContextualAnimTrack* AnimTrackPtr = SceneAsset.GetAnimTrack(SectionIdx, AnimSetIdxSelected, Pair.Key);
			check(AnimTrackPtr);

			OutBindings.Data.Add(FContextualAnimSceneBinding(Pair.Value, *AnimTrackPtr));
		}

		check(OutBindings.IsValid());
		return true;
	}

	return false;
}

bool FContextualAnimSceneBindings::TryCreateBindings(const UContextualAnimSceneAsset& SceneAsset, int32 SectionIdx, const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Secondary, FContextualAnimSceneBindings& OutBindings)
{
	check(SceneAsset.HasValidData());

	OutBindings.Reset();

	const TArray<FContextualAnimRoleDefinition>& Roles = SceneAsset.GetRolesAsset()->Roles;
	if (Roles.Num() > 2)
	{
		UE_LOG(LogContextualAnim, Warning, TEXT("FContextualAnimSceneBindings::TryCreateBindings Failed. Reason: Trying to create bindings with two actors for a SceneAsset with more than two roles. SceneAsset: %s Num Roles: %d SectionIdx: %d"),
			*GetNameSafe(&SceneAsset), Roles.Num(), SectionIdx);

		return false;
	}

	TMap<FName, FContextualAnimSceneBindingContext> Params;
	for (const FContextualAnimRoleDefinition& RoleDef : Roles)
	{
		const FName PrimaryRole = SceneAsset.GetPrimaryRole();
		Params.Add(RoleDef.Name, (RoleDef.Name == PrimaryRole) ? Primary : Secondary);
	}

	return TryCreateBindings(SceneAsset, SectionIdx, Params, OutBindings);
}

void FContextualAnimSceneBindings::CalculateAnimSetPivots(TArray<FContextualAnimSetPivot>& OutScenePivots) const
{
	if (IsValid())
	{
		for (const FContextualAnimSetPivotDefinition& Def : SceneAsset->GetAnimSetPivotDefinitionsInSection(SectionIdx))
		{
			FContextualAnimSetPivot& ScenePivotRuntime = OutScenePivots.AddDefaulted_GetRef();
			CalculateAnimSetPivot(Def, ScenePivotRuntime);
		}
	}
}

bool FContextualAnimSceneBindings::CalculateAnimSetPivot(const FContextualAnimSetPivotDefinition& AnimSetPivotDef, FContextualAnimSetPivot& OutScenePivot) const
{
	if (const FContextualAnimSceneBinding* Binding = FindBindingByRole(AnimSetPivotDef.Origin))
	{
		OutScenePivot.Name = AnimSetPivotDef.Name;
		if (AnimSetPivotDef.bAlongClosestDistance)
		{
			if (const FContextualAnimSceneBinding* OtherBinding = FindBindingByRole(AnimSetPivotDef.OtherRole))
			{
				const FTransform T1 = Binding->GetTransform();
				const FTransform T2 = OtherBinding->GetTransform();

				OutScenePivot.Transform.SetLocation(FMath::Lerp<FVector>(T1.GetLocation(), T2.GetLocation(), AnimSetPivotDef.Weight));
				OutScenePivot.Transform.SetRotation((T2.GetLocation() - T1.GetLocation()).GetSafeNormal2D().ToOrientationQuat());
				return true;
			}
		}
		else
		{
			OutScenePivot.Transform = Binding->GetTransform();
			return true;
		}
	}

	return false;
}
