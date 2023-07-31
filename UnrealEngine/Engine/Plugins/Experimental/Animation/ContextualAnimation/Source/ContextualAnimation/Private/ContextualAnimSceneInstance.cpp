// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimSceneInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "ContextualAnimSceneAsset.h"
#include "ContextualAnimSceneActorComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContextualAnimSceneInstance)

// UContextualAnimSceneInstance
//================================================================================================================

UContextualAnimSceneInstance::UContextualAnimSceneInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UWorld* UContextualAnimSceneInstance::GetWorld()const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UContextualAnimSceneInstance::Tick(const float DeltaTime)
{
	RemainingDuration -= DeltaTime;
	if (RemainingDuration <= 0.f)
	{
		RemainingDuration = MAX_flt;
		OnSectionEndTimeReached.Broadcast(this);
	}
}

bool UContextualAnimSceneInstance::IsActorInThisScene(const AActor* Actor) const
{
	return FindBindingByActor(Actor) != nullptr;
}

AActor* UContextualAnimSceneInstance::GetActorByRole(FName Role) const
{
	const FContextualAnimSceneBinding* Binding = FindBindingByRole(Role);
	return Binding ? Binding->GetActor() : nullptr;
}

UAnimMontage* UContextualAnimSceneInstance::PlayAnimation(UAnimInstance& AnimInstance, UAnimSequenceBase& Animation)
{
	if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(&Animation))
	{
		const float Duration = AnimInstance.Montage_Play(AnimMontage, 1.f, EMontagePlayReturnType::MontageLength);
		return (Duration > 0.f) ? AnimMontage : nullptr;
	}
	else
	{
		//@TODO: Expose all these on the AnimTrack
		const FName SlotName = FName(TEXT("DefaultSlot"));
		const float BlendInTime = 0.25f;
		const float BlendOutTime = 0.25f;
		const float InPlayRate = 1.f;
		const int32 LoopCount = 1;
		const float BlendOutTriggerTime = -1.f;
		const float InTimeToStartMontageAt = 0.f;
		return AnimInstance.PlaySlotAnimationAsDynamicMontage(&Animation, SlotName, BlendInTime, BlendOutTime, InPlayRate, LoopCount, BlendOutTriggerTime, InTimeToStartMontageAt);
	}
}

float UContextualAnimSceneInstance::Join(FContextualAnimSceneBinding& Binding)
{
	float Duration = MIN_flt;

	AActor* Actor = Binding.GetActor();
	if (Actor == nullptr)
	{
		return Duration;
	}

	if (UAnimSequenceBase* Animation = Bindings.GetAnimTrackFromBinding(Binding).Animation)
	{
		if (UAnimInstance* AnimInstance = Binding.GetAnimInstance())
		{
			if (const UAnimMontage* Montage = PlayAnimation(*AnimInstance, *Animation))
			{
				AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UContextualAnimSceneInstance::OnNotifyBeginReceived);
				AnimInstance->OnPlayMontageNotifyEnd.AddUniqueDynamic(this, &UContextualAnimSceneInstance::OnNotifyEndReceived);
				AnimInstance->OnMontageBlendingOut.AddUniqueDynamic(this, &UContextualAnimSceneInstance::OnMontageBlendingOut);

				const float AdjustedPlayRate = AnimInstance->Montage_GetPlayRate(Montage) * Montage->RateScale;				
				if (AdjustedPlayRate > 0.f)
				{
					Duration = (Montage->GetPlayLength() / AdjustedPlayRate);	
				}
				else
				{
					UE_LOG(LogContextualAnim, Warning, TEXT("Undesired playrate %.3f, using montage play length instead."), AdjustedPlayRate);
					Duration = Montage->GetPlayLength();
				}
			}
		}

		//@TODO: Temp, until we have a way to switch between movement mode using AnimNotifyState
		if (Bindings.GetAnimTrackFromBinding(Binding).bRequireFlyingMode)
		{
			if (UCharacterMovementComponent* CharacterMovementComp = Actor->FindComponentByClass<UCharacterMovementComponent>())
			{
				CharacterMovementComp->SetMovementMode(MOVE_Flying);
			}
		}
	}

	UMotionWarpingComponent* MotionWarpComp = Actor->FindComponentByClass<UMotionWarpingComponent>();
	for (int32 PivotIndex = 0; PivotIndex < AlignmentSectionToScenePivotList.Num(); PivotIndex++)
	{
		const FContextualAnimSetPivot& Pivot = AlignmentSectionToScenePivotList[PivotIndex];
		const float Time = Bindings.GetAnimTrackFromBinding(Binding).GetSyncTimeForWarpSection(Pivot.Name);

		if (MotionWarpComp)
		{
			const FTransform TransformRelativeToScenePivot = Bindings.GetAlignmentTransformFromBinding(Binding, Pivot.Name, Time);
			const FTransform WarpTarget = TransformRelativeToScenePivot * Pivot.Transform;
			MotionWarpComp->AddOrUpdateWarpTargetFromTransform(Pivot.Name, WarpTarget);
		}
		else if (PivotIndex == 0)
		{
			// In case motion warping is not available, we use the first alignment section at time T=0 to teleport the actor
			const FTransform TransformRelativeToScenePivot = Bindings.GetAlignmentTransformFromBinding(Binding, Pivot.Name, 0.f);
			const FTransform ActorTransform = TransformRelativeToScenePivot * Pivot.Transform;
			Actor->TeleportTo(ActorTransform.GetLocation(), ActorTransform.GetRotation().Rotator(), /*bIsATest*/false, /*bNoCheck*/true);
		}
	}

	if (SceneAsset->GetDisableCollisionBetweenActors())
	{
		SetIgnoreCollisionWithOtherActors(Actor, true);
	}

	if (UContextualAnimSceneActorComponent* SceneActorComp = Binding.GetSceneActorComponent())
	{
		SceneActorComp->OnJoinedScene(Bindings);
	}

	OnActorJoined.Broadcast(this, Actor);

	return Duration;
}

void UContextualAnimSceneInstance::Leave(FContextualAnimSceneBinding& Binding)
{
	if (UAnimInstance* AnimInstance = Binding.GetAnimInstance())
	{
		if (const UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage())
		{
			AnimInstance->Montage_Stop(CurrentMontage->BlendOut.GetBlendTime(), CurrentMontage);
		}
	}
}

float UContextualAnimSceneInstance::TransitionTo(FContextualAnimSceneBinding& Binding, const FContextualAnimTrack& AnimTrack)
{
	float Duration = MIN_flt;

	UAnimInstance* AnimInstance = Binding.GetAnimInstance();
	if (AnimInstance == nullptr)
	{		
		return Duration;
	}

	// Unbind blend out delegate for a moment so we don't get it during the transition
	// @TODO: Replace this with the TGuardValue 'pattern', similar to what we do in the editor for OnAnimNotifyChanged
	AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &UContextualAnimSceneInstance::OnMontageBlendingOut);

	const UAnimMontage* Montage = PlayAnimation(*AnimInstance, *AnimTrack.Animation);
	Binding.SetAnimTrack(AnimTrack);

	AnimInstance->OnMontageBlendingOut.AddUniqueDynamic(this, &UContextualAnimSceneInstance::OnMontageBlendingOut);

	if (Montage != nullptr)
	{
		const float AdjustedPlayRate = AnimInstance->Montage_GetPlayRate(Montage) * Montage->RateScale;				
		if (AdjustedPlayRate > 0.f)
		{
			Duration = (Montage->GetPlayLength() / AdjustedPlayRate);	
		}
		else
		{
			UE_LOG(LogContextualAnim, Warning, TEXT("Undesired playrate %.3f, using montage play length instead."), AdjustedPlayRate);
			Duration = Montage->GetPlayLength();
		}
	}

	return Duration;
}

void UContextualAnimSceneInstance::Start()
{
	RemainingDuration = 0.f;

	for (auto& Binding : Bindings)
	{
		const float TrackDuration = Join(Binding);
		RemainingDuration = FMath::Max(RemainingDuration, TrackDuration);
	}
}

void UContextualAnimSceneInstance::Stop()
{
	for (auto& Binding : Bindings)
	{
		Leave(Binding);
	}
}

bool UContextualAnimSceneInstance::IsDonePlaying() const
{
	return RemainingDuration == MAX_flt;
}

bool UContextualAnimSceneInstance::ForceTransitionToSection(const int32 SectionIdx, const int32 AnimSetIdx, const TArray<FContextualAnimSetPivot>& Pivots)
{
	if (!IsValid(SceneAsset))
	{
		UE_LOG(LogContextualAnim, Error, TEXT("Failed transition to section '%d': invalid scene asset"), SectionIdx);
		return false;
	}

	const int32 NumSections = SceneAsset->GetNumSections();
	if (NumSections == 0)
	{
		UE_LOG(LogContextualAnim, Error, TEXT("Failed transition to section '%d': no sections defined in asset %s"), SectionIdx, *SceneAsset->GetName());
		return false;
	}

	if (SectionIdx < 0 || SectionIdx >= NumSections)
	{
		UE_LOG(LogContextualAnim, Error, TEXT("Failed transition to section '%d': index not in valid range [0, %d] for asset %s"), SectionIdx, NumSections, *SceneAsset->GetName());
		return false;
	}

	if (SectionIdx == Bindings.GetSectionIdx())
	{
		UE_LOG(LogContextualAnim, Error, TEXT("Failed transition to section '%d': section is already playing"), SectionIdx);
		return false;
	}

	const FName PrimaryRole = SceneAsset->GetPrimaryRole();
	const FContextualAnimSceneBinding* PrimaryRoleBinding = Bindings.FindBindingByRole(PrimaryRole);
	if (PrimaryRoleBinding == nullptr)
	{
		UE_LOG(LogContextualAnim, Error, TEXT("Failed transition to section '%d': unable to find scene binding for primary role: %s"), SectionIdx, *PrimaryRole.ToString());
		return false;
	}

	SetPivots(Pivots);
	
	RemainingDuration = 0.f;
	for (FContextualAnimSceneBinding& Binding : Bindings)
	{
		const FContextualAnimTrack* AnimTrack = SceneAsset->GetAnimTrack(SectionIdx, AnimSetIdx, Bindings.GetRoleFromBinding(Binding));
		if (AnimTrack == nullptr)
		{
			return false;
		}

		const float TrackDuration = TransitionTo(Binding, *AnimTrack);
		RemainingDuration = FMath::Max(RemainingDuration, TrackDuration);
	}

	return true;
}

void UContextualAnimSceneInstance::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	UE_LOG(LogContextualAnim, Log, TEXT("UContextualAnimSceneInstance::OnMontageBlendingOut Montage: %s"), *GetNameSafe(Montage));

	for (auto& Binding : Bindings)
	{
		const FContextualAnimTrack& AnimTrack = Bindings.GetAnimTrackFromBinding(Binding);
		if (AnimTrack.Animation == Montage)
		{
			AActor* Actor = Binding.GetActor();
			if (UAnimInstance* AnimInstance = Binding.GetAnimInstance())
			{
				AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UContextualAnimSceneInstance::OnNotifyBeginReceived);
				AnimInstance->OnPlayMontageNotifyEnd.RemoveDynamic(this, &UContextualAnimSceneInstance::OnNotifyEndReceived);
				AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &UContextualAnimSceneInstance::OnMontageBlendingOut);

				if (AnimTrack.bRequireFlyingMode)
				{
					if (UCharacterMovementComponent* CharacterMovementComp = Actor->FindComponentByClass<UCharacterMovementComponent>())
					{
						CharacterMovementComp->SetMovementMode(MOVE_Walking);
					}
				}
			}

			if (SceneAsset->GetDisableCollisionBetweenActors())
			{
				SetIgnoreCollisionWithOtherActors(Binding.GetActor(), false);
			}

			if (UContextualAnimSceneActorComponent* SceneActorComp = Binding.GetSceneActorComponent())
			{
				SceneActorComp->OnLeftScene();
			}

			OnActorLeft.Broadcast(this, Actor);

			break;
		}
	}

	bool bShouldEnd = true;
	for (auto& Binding : Bindings)
	{
		if (UAnimInstance* AnimInstance = Binding.GetAnimInstance())
		{
			const FContextualAnimTrack& AnimTrack = Bindings.GetAnimTrackFromBinding(Binding);

			// Keep montage support for now but might go away soon
			if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimTrack.Animation))
			{
				if (AnimInstance->Montage_IsPlaying(AnimMontage))
				{
					bShouldEnd = false;
					break;
				}
			}
			else
			{
				for (const FAnimMontageInstance* MontageInstance : AnimInstance->MontageInstances)
				{
					// When the animation is not a Montage, we still play it as a Montage. This dynamically created Montage has a single slot and single segment.
					if (MontageInstance && MontageInstance->IsPlaying())
					{
						if(MontageInstance->Montage->SlotAnimTracks.Num() > 0 && MontageInstance->Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
						{
							if (MontageInstance->Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference() == AnimTrack.Animation)
							{
								bShouldEnd = false;
								break;
							}
						}
					}
				}
			}
		}
	}

	if (bShouldEnd)
	{
		OnSceneEnded.Broadcast(this);

		// Won't be ticked anymore so make sure to notify OnSectionEndTimeReached if necessary
		if (RemainingDuration != MAX_flt)
		{
			RemainingDuration = MAX_flt;
			OnSectionEndTimeReached.Broadcast(this);
		}
	}
}

void UContextualAnimSceneInstance::OnNotifyBeginReceived(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
	UE_LOG(LogContextualAnim, Log, TEXT("UContextualAnimSceneInstance::OnNotifyBeginReceived NotifyName: %s Montage: %s"),
	*NotifyName.ToString(), *GetNameSafe(BranchingPointNotifyPayload.SequenceAsset));

	if(const USkeletalMeshComponent* SkelMeshCom = BranchingPointNotifyPayload.SkelMeshComponent)
	{
		OnNotifyBegin.Broadcast(this, SkelMeshCom->GetOwner(), NotifyName);
	}
}

void UContextualAnimSceneInstance::OnNotifyEndReceived(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
	UE_LOG(LogContextualAnim, Log, TEXT("UContextualAnimSceneInstance::OnNotifyEndReceived NotifyName: %s Montage: %s"),
	*NotifyName.ToString(), *GetNameSafe(BranchingPointNotifyPayload.SequenceAsset));

	if (const USkeletalMeshComponent* SkelMeshCom = BranchingPointNotifyPayload.SkelMeshComponent)
	{
		OnNotifyEnd.Broadcast(this, SkelMeshCom->GetOwner(), NotifyName);
	}
}

void UContextualAnimSceneInstance::SetIgnoreCollisionWithOtherActors(AActor* Actor, bool bValue) const
{
	for (auto& Binding : Bindings)
	{
		AActor* OtherActor = Binding.GetActor();
		if(OtherActor != Actor)
		{
			if (UPrimitiveComponent* RootPrimitiveComponent = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
			{
				RootPrimitiveComponent->IgnoreActorWhenMoving(OtherActor, bValue);
			}
		}
	}
}

