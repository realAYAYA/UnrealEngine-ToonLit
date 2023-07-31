// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionWarpingComponent.h"

#include "RootMotionModifier.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationPoseData.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MotionWarpingComponent)

DEFINE_LOG_CATEGORY(LogMotionWarping);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
TAutoConsoleVariable<int32> FMotionWarpingCVars::CVarMotionWarpingDisable(TEXT("a.MotionWarping.Disable"), 0, TEXT("Disable Motion Warping"), ECVF_Cheat);
TAutoConsoleVariable<int32> FMotionWarpingCVars::CVarMotionWarpingDebug(TEXT("a.MotionWarping.Debug"), 0, TEXT("0: Disable, 1: Only Log, 2: Only DrawDebug, 3: Log and DrawDebug"), ECVF_Cheat);
TAutoConsoleVariable<float> FMotionWarpingCVars::CVarMotionWarpingDrawDebugDuration(TEXT("a.MotionWarping.DrawDebugLifeTime"), 1.f, TEXT("Time in seconds each draw debug persists.\nRequires 'a.MotionWarping.Debug 2'"), ECVF_Cheat);
#endif

// UMotionWarpingUtilities
///////////////////////////////////////////////////////////////////////

void UMotionWarpingUtilities::ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose)
{
	OutPose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	FAnimExtractContext Context(Time, bExtractRootMotion);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(OutPose, Curve, Attributes);
	if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		AnimSequence->GetBonePose(AnimationPoseData, Context);
	}
	else if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		const FAnimTrack& AnimTrack = AnimMontage->SlotAnimTracks[0].AnimTrack;
		AnimTrack.GetAnimationPose(AnimationPoseData, Context);
	}
}

void UMotionWarpingUtilities::ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose)
{
	FCompactPose Pose;
	ExtractLocalSpacePose(Animation, BoneContainer, Time, bExtractRootMotion, Pose);
	OutPose.InitPose(MoveTemp(Pose));
}

FTransform UMotionWarpingUtilities::ExtractRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime)
{
	if (const UAnimMontage* Anim = Cast<UAnimMontage>(Animation))
	{
		// This is identical to UAnimMontage::ExtractRootMotionFromTrackRange and UAnimCompositeBase::ExtractRootMotionFromTrack but ignoring bEnableRootMotion
		// so we can extract root motion from the montage even if that flag is set to false in the AnimSequence(s)

		FRootMotionMovementParams AccumulatedRootMotionParams;

		if (Anim->SlotAnimTracks.Num() > 0)
		{
			const FAnimTrack& RootMotionAnimTrack = Anim->SlotAnimTracks[0].AnimTrack;

			TArray<FRootMotionExtractionStep> RootMotionExtractionSteps;
			RootMotionAnimTrack.GetRootMotionExtractionStepsForTrackRange(RootMotionExtractionSteps, StartTime, EndTime);

			for (const FRootMotionExtractionStep& CurStep : RootMotionExtractionSteps)
			{
				if (CurStep.AnimSequence)
				{
					AccumulatedRootMotionParams.Accumulate(CurStep.AnimSequence->ExtractRootMotionFromRange(CurStep.StartPosition, CurStep.EndPosition));
				}
			}
		}

		return AccumulatedRootMotionParams.GetRootMotionTransform();
	}

	if (const UAnimSequence* Anim = Cast<UAnimSequence>(Animation))
	{
		return Anim->ExtractRootMotionFromRange(StartTime, EndTime);
	}

	return FTransform::Identity;
}

FTransform UMotionWarpingUtilities::ExtractRootTransformFromAnimation(const UAnimSequenceBase* Animation, float Time)
{
	if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		if(const FAnimSegment* Segment = AnimMontage->SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(Time))
		{
			if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Segment->GetAnimReference()))
			{
				const float AnimSequenceTime = Segment->ConvertTrackPosToAnimPos(Time);
				return AnimSequence->ExtractRootTrackTransform(AnimSequenceTime, nullptr);
			}	
		}
	}
	else if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		return AnimSequence->ExtractRootTrackTransform(Time, nullptr);
	}

	return FTransform::Identity;
}

void UMotionWarpingUtilities::GetMotionWarpingWindowsFromAnimation(const UAnimSequenceBase* Animation, TArray<FMotionWarpingWindowData>& OutWindows)
{
	if(Animation)
	{
		OutWindows.Reset();

		for (int32 Idx = 0; Idx < Animation->Notifies.Num(); Idx++)
		{
			const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Idx];
			if (UAnimNotifyState_MotionWarping* Notify = Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass))
			{
				FMotionWarpingWindowData Data;
				Data.AnimNotify = Notify;
				Data.StartTime = NotifyEvent.GetTriggerTime();
				Data.EndTime = NotifyEvent.GetEndTriggerTime();
				OutWindows.Add(Data);
			}
		}
	}
}

void UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(const UAnimSequenceBase* Animation, FName WarpTargetName, TArray<FMotionWarpingWindowData>& OutWindows)
{
	if (Animation && WarpTargetName != NAME_None)
	{
		OutWindows.Reset();

		for (int32 Idx = 0; Idx < Animation->Notifies.Num(); Idx++)
		{
			const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Idx];
			if (UAnimNotifyState_MotionWarping* Notify = Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass))
			{
				if (const URootMotionModifier_Warp* Modifier = Cast<const URootMotionModifier_Warp>(Notify->RootMotionModifier))
				{
					if(Modifier->WarpTargetName == WarpTargetName)
					{
						FMotionWarpingWindowData Data;
						Data.AnimNotify = Notify;
						Data.StartTime = NotifyEvent.GetTriggerTime();
						Data.EndTime = NotifyEvent.GetEndTriggerTime();
						OutWindows.Add(Data);
					}
				}
			}
		}
	}
}

FTransform UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(const ACharacter& Character, const UAnimSequenceBase* Animation, float Time, const FName& WarpPointBoneName)
{
	if (const USkeletalMeshComponent* Mesh = Character.GetMesh())
	{
		if (const UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			const FBoneContainer& FullBoneContainer = AnimInstance->GetRequiredBones();
			const int32 BoneIndex = FullBoneContainer.GetPoseBoneIndexForBoneName(WarpPointBoneName);
			if (ensure(BoneIndex != INDEX_NONE))
			{
				TArray<FBoneIndexType> RequiredBoneIndexArray = { 0, (FBoneIndexType)BoneIndex };
				FullBoneContainer.GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBoneIndexArray);

				FBoneContainer LimitedBoneContainer(RequiredBoneIndexArray, FCurveEvaluationOption(false), *FullBoneContainer.GetAsset());

				FCSPose<FCompactPose> Pose;
				UMotionWarpingUtilities::ExtractComponentSpacePose(Animation, LimitedBoneContainer, Time, false, Pose);

				// Inverse of mesh's relative rotation. Used to convert root and warp point in the animation from Y forward to X forward
				const FTransform MeshCompRelativeRotInverse = FTransform(Character.GetBaseRotationOffset().Inverse());

				const FTransform RootTransform = MeshCompRelativeRotInverse * Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(0));
				const FTransform WarpPointTransform = MeshCompRelativeRotInverse * Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(1));
				return RootTransform.GetRelativeTransform(WarpPointTransform);
			}
		}
	}

	return FTransform::Identity;
}

FTransform UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(const ACharacter& Character, const UAnimSequenceBase* Animation, float Time, const FTransform& WarpPointTransform)
{
	// Inverse of mesh's relative rotation. Used to convert root and warp point in the animation from Y forward to X forward
	const FTransform MeshCompRelativeRotInverse = FTransform(Character.GetBaseRotationOffset().Inverse());
	const FTransform RootTransform = MeshCompRelativeRotInverse * UMotionWarpingUtilities::ExtractRootTransformFromAnimation(Animation, Time);
	return RootTransform.GetRelativeTransform((MeshCompRelativeRotInverse * WarpPointTransform));
}

// UMotionWarpingComponent
///////////////////////////////////////////////////////////////////////

UMotionWarpingComponent::UMotionWarpingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
}

void UMotionWarpingComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_SimulatedOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UMotionWarpingComponent, WarpTargets, Params);
}

void UMotionWarpingComponent::InitializeComponent()
{
	Super::InitializeComponent();

	CharacterOwner = Cast<ACharacter>(GetOwner());

	UCharacterMovementComponent* CharacterMovementComp = CharacterOwner.IsValid() ? CharacterOwner->GetCharacterMovement() : nullptr;
	if (CharacterMovementComp)
	{
 		CharacterMovementComp->ProcessRootMotionPreConvertToWorld.BindUObject(this, &UMotionWarpingComponent::ProcessRootMotionPreConvertToWorld);
	}
}

bool UMotionWarpingComponent::ContainsModifier(const UAnimSequenceBase* Animation, float StartTime, float EndTime) const
{
	return Modifiers.ContainsByPredicate([=](const URootMotionModifier* Modifier)
		{
			return (Modifier->Animation == Animation && Modifier->StartTime == StartTime && Modifier->EndTime == EndTime);
		});
}

int32 UMotionWarpingComponent::AddModifier(URootMotionModifier* Modifier)
{
	if (ensureAlways(Modifier))
	{
		UE_LOG(LogMotionWarping, Verbose, TEXT("MotionWarping: RootMotionModifier added. NetMode: %d WorldTime: %f Char: %s Animation: %s [%f %f] [%f %f] Loc: %s Rot: %s"),
			GetWorld()->GetNetMode(), GetWorld()->GetTimeSeconds(), *GetNameSafe(GetCharacterOwner()), *GetNameSafe(Modifier->Animation.Get()), Modifier->StartTime, Modifier->EndTime, Modifier->PreviousPosition, Modifier->CurrentPosition,
			*GetCharacterOwner()->GetActorLocation().ToString(), *GetCharacterOwner()->GetActorRotation().ToCompactString());

		return Modifiers.Add(Modifier);
	}

	return INDEX_NONE;
}

void UMotionWarpingComponent::DisableAllRootMotionModifiers()
{
	if (Modifiers.Num() > 0)
	{
		for (URootMotionModifier* Modifier : Modifiers)
		{
			Modifier->SetState(ERootMotionModifierState::Disabled);
		}
	}
}

void UMotionWarpingComponent::Update(float DeltaSeconds)
{
	const ACharacter* Character = GetCharacterOwner();
	check(Character);

	FMotionWarpingUpdateContext Context;
	Context.DeltaSeconds = DeltaSeconds;

	// When replaying saved moves we need to look at the contributor to root motion back then.
	if (Character->bClientUpdating)
	{
		const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
		check(MoveComp);

		const FSavedMove_Character* SavedMove = MoveComp->GetCurrentReplayedSavedMove();
		check(SavedMove);

		if(SavedMove->RootMotionMontage.IsValid())
		{
			Context.Animation = SavedMove->RootMotionMontage.Get();
			Context.CurrentPosition = SavedMove->RootMotionTrackPosition;
			Context.PreviousPosition = SavedMove->RootMotionPreviousTrackPosition;
			Context.PlayRate = SavedMove->RootMotionPlayRateWithScale;
		}
	}
	else // If we are not replaying a move, just use the current root motion montage
	{
		if(const FAnimMontageInstance* RootMotionMontageInstance = Character->GetRootMotionAnimMontageInstance())
		{
			const UAnimMontage* Montage = RootMotionMontageInstance->Montage;
			check(Montage);

			Context.Animation = Montage;
			Context.CurrentPosition = RootMotionMontageInstance->GetPosition();
			Context.PreviousPosition = RootMotionMontageInstance->GetPreviousPosition();
			Context.Weight = RootMotionMontageInstance->GetWeight();
			Context.PlayRate = RootMotionMontageInstance->Montage->RateScale * RootMotionMontageInstance->GetPlayRate();
		}
	}

	if (Context.Animation.IsValid())
	{
		const UAnimSequenceBase* Animation = Context.Animation.Get();
		const float PreviousPosition = Context.PreviousPosition;
		const float CurrentPosition = Context.CurrentPosition;

		// Loop over notifies directly in the montage, looking for Motion Warping windows
		for (const FAnimNotifyEvent& NotifyEvent : Animation->Notifies)
		{
			const UAnimNotifyState_MotionWarping* MotionWarpingNotify = NotifyEvent.NotifyStateClass ? Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass) : nullptr;
			if (MotionWarpingNotify)
			{
				if(MotionWarpingNotify->RootMotionModifier == nullptr)
				{
					UE_LOG(LogMotionWarping, Warning, TEXT("MotionWarpingComponent::Update. A motion warping window in %s doesn't have a valid root motion modifier!"), *GetNameSafe(Animation));
					continue;
				}

				const float StartTime = FMath::Clamp(NotifyEvent.GetTriggerTime(), 0.f, Animation->GetPlayLength());
				const float EndTime = FMath::Clamp(NotifyEvent.GetEndTriggerTime(), 0.f, Animation->GetPlayLength());

				if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
				{
					if (!ContainsModifier(Animation, StartTime, EndTime))
					{
						MotionWarpingNotify->OnBecomeRelevant(this, Animation, StartTime, EndTime);
					}
				}
			}
		}

		if(bSearchForWindowsInAnimsWithinMontages)
		{
			if(const UAnimMontage* Montage = Cast<const UAnimMontage>(Context.Animation.Get()))
			{
				// Same as before but scanning all animation within the montage
				for (int32 SlotIdx = 0; SlotIdx < Montage->SlotAnimTracks.Num(); SlotIdx++)
				{
					const FAnimTrack& AnimTrack = Montage->SlotAnimTracks[SlotIdx].AnimTrack;

					if (const FAnimSegment* AnimSegment = AnimTrack.GetSegmentAtTime(PreviousPosition))
					{
						if (const UAnimSequenceBase* AnimReference = AnimSegment->GetAnimReference())
						{
							for (const FAnimNotifyEvent& NotifyEvent : AnimReference->Notifies)
							{
								const UAnimNotifyState_MotionWarping* MotionWarpingNotify = NotifyEvent.NotifyStateClass ? Cast<UAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass) : nullptr;
								if (MotionWarpingNotify)
								{
									if (MotionWarpingNotify->RootMotionModifier == nullptr)
									{
										UE_LOG(LogMotionWarping, Warning, TEXT("MotionWarpingComponent::Update. A motion warping window in %s doesn't have a valid root motion modifier!"), *GetNameSafe(AnimReference));
										continue;
									}

									const float NotifyStartTime = FMath::Clamp(NotifyEvent.GetTriggerTime(), 0.f, AnimReference->GetPlayLength());
									const float NotifyEndTime = FMath::Clamp(NotifyEvent.GetEndTriggerTime(), 0.f, AnimReference->GetPlayLength());

									// Convert notify times from AnimSequence times to montage times
									const float StartTime = (NotifyStartTime - AnimSegment->AnimStartTime) + AnimSegment->StartPos;
									const float EndTime = (NotifyEndTime - AnimSegment->AnimStartTime) + AnimSegment->StartPos;

									if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
									{
										if (!ContainsModifier(Montage, StartTime, EndTime))
										{
											MotionWarpingNotify->OnBecomeRelevant(this, Montage, StartTime, EndTime);
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

	OnPreUpdate.Broadcast(this);

	// Update the state of all the modifiers
	if (Modifiers.Num() > 0)
	{
		for (URootMotionModifier* Modifier : Modifiers)
		{
			Modifier->Update(Context);
		}

		// Remove the modifiers that has been marked for removal
		Modifiers.RemoveAll([this](const URootMotionModifier* Modifier)
		{
			if (Modifier->GetState() == ERootMotionModifierState::MarkedForRemoval)
			{
				UE_LOG(LogMotionWarping, Verbose, TEXT("MotionWarping: RootMotionModifier removed. NetMode: %d WorldTime: %f Char: %s Animation: %s [%f %f] [%f %f] Loc: %s Rot: %s"),
					GetWorld()->GetNetMode(), GetWorld()->GetTimeSeconds(), *GetNameSafe(GetCharacterOwner()), *GetNameSafe(Modifier->Animation.Get()), Modifier->StartTime, Modifier->EndTime, Modifier->PreviousPosition, Modifier->CurrentPosition,
					*GetCharacterOwner()->GetActorLocation().ToString(), *GetCharacterOwner()->GetActorRotation().ToCompactString());

				return true;
			}

			return false;
		});
	}
}

FTransform UMotionWarpingComponent::ProcessRootMotionPreConvertToWorld(const FTransform& InRootMotion, UCharacterMovementComponent* CharacterMovementComponent, float DeltaSeconds)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FMotionWarpingCVars::CVarMotionWarpingDisable.GetValueOnGameThread() > 0)
	{
		return InRootMotion;
	}
#endif

	// Check for warping windows and update modifier states
	Update(DeltaSeconds);

	FTransform FinalRootMotion = InRootMotion;

	// Apply Local Space Modifiers
	for (URootMotionModifier* Modifier : Modifiers)
	{
		if (Modifier->GetState() == ERootMotionModifierState::Active)
		{
			FinalRootMotion = Modifier->ProcessRootMotion(FinalRootMotion, DeltaSeconds);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 DebugLevel = FMotionWarpingCVars::CVarMotionWarpingDebug.GetValueOnGameThread();
	if (DebugLevel >= 2)
	{
		const float DrawDebugDuration = FMotionWarpingCVars::CVarMotionWarpingDrawDebugDuration.GetValueOnGameThread();
		const float PointSize = 7.f;
		const FVector ActorFeetLocation = CharacterMovementComponent->GetActorFeetLocation();
		if (Modifiers.Num() > 0)
		{
			if (!OriginalRootMotionAccum.IsSet())
			{
				OriginalRootMotionAccum = ActorFeetLocation;
				WarpedRootMotionAccum = ActorFeetLocation;
			}

			OriginalRootMotionAccum = OriginalRootMotionAccum.GetValue() + (CharacterOwner->GetMesh()->ConvertLocalRootMotionToWorld(FTransform(InRootMotion.GetLocation()))).GetLocation();
			WarpedRootMotionAccum = WarpedRootMotionAccum.GetValue() + (CharacterOwner->GetMesh()->ConvertLocalRootMotionToWorld(FTransform(FinalRootMotion.GetLocation()))).GetLocation();

			DrawDebugPoint(GetWorld(), OriginalRootMotionAccum.GetValue(), PointSize, FColor::Red, false, DrawDebugDuration, 0);
			DrawDebugPoint(GetWorld(), WarpedRootMotionAccum.GetValue(), PointSize, FColor::Green, false, DrawDebugDuration, 0);
		}
		else
		{
			OriginalRootMotionAccum.Reset();
			WarpedRootMotionAccum.Reset();
		}

		DrawDebugPoint(GetWorld(), ActorFeetLocation, PointSize, FColor::Blue, false, DrawDebugDuration, 0);
	}
#endif

	return FinalRootMotion;
}

bool UMotionWarpingComponent::FindAndUpdateWarpTarget(const FMotionWarpingTarget& WarpTarget)
{
	for (int32 Idx = 0; Idx < WarpTargets.Num(); Idx++)
	{
		if (WarpTargets[Idx].Name == WarpTarget.Name)
		{
			WarpTargets[Idx] = WarpTarget;
			return true;
		}
	}

	return false;
}

void UMotionWarpingComponent::AddOrUpdateWarpTarget(const FMotionWarpingTarget& WarpTarget)
{
	if (WarpTarget.Name != NAME_None)
	{
		// if we did not find the target, add it
		if (!FindAndUpdateWarpTarget(WarpTarget))
		{
			WarpTargets.Add(WarpTarget);
		}

		MARK_PROPERTY_DIRTY_FROM_NAME(UMotionWarpingComponent, WarpTargets, this);
	}
}

int32 UMotionWarpingComponent::RemoveWarpTarget(FName WarpTargetName)
{
	const int32 NumRemoved = WarpTargets.RemoveAll([&WarpTargetName](const FMotionWarpingTarget& WarpTarget) { return WarpTarget.Name == WarpTargetName; });
	
	if(NumRemoved > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UMotionWarpingComponent, WarpTargets, this);
	}

	return NumRemoved;
}

void UMotionWarpingComponent::AddOrUpdateWarpTargetFromTransform(FName WarpTargetName, FTransform TargetTransform)
{
	AddOrUpdateWarpTarget(FMotionWarpingTarget(WarpTargetName, TargetTransform));
}

void UMotionWarpingComponent::AddOrUpdateWarpTargetFromComponent(FName WarpTargetName, const USceneComponent* Component, FName BoneName, bool bFollowComponent)
{
	if (Component == nullptr)
	{
		UE_LOG(LogMotionWarping, Warning, TEXT("AddOrUpdateWarpTargetFromComponent has failed!. Reason: Invalid Component"));
		return;
	}

	AddOrUpdateWarpTarget(FMotionWarpingTarget(WarpTargetName, Component, BoneName, bFollowComponent));
}

URootMotionModifier* UMotionWarpingComponent::AddModifierFromTemplate(URootMotionModifier* Template, const UAnimSequenceBase* Animation, float StartTime, float EndTime)
{
	if (ensureAlways(Template))
	{
		FObjectDuplicationParameters Params(Template, this);
		URootMotionModifier* NewRootMotionModifier = CastChecked<URootMotionModifier>(StaticDuplicateObjectEx(Params));
		
		NewRootMotionModifier->Animation = Animation;
		NewRootMotionModifier->StartTime = StartTime;
		NewRootMotionModifier->EndTime = EndTime;

		AddModifier(NewRootMotionModifier);

		return NewRootMotionModifier;
	}

	return nullptr;
}
