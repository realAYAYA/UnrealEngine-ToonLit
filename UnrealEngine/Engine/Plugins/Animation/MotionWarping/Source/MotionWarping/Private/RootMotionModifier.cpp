// Copyright Epic Games, Inc. All Rights Reserved.

#include "RootMotionModifier.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "MotionWarpingComponent.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "AnimNotifyState_MotionWarping.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RootMotionModifier)

// FMotionWarpingTarget
///////////////////////////////////////////////////////////////

FMotionWarpingTarget::FMotionWarpingTarget(const FName& InName, const USceneComponent* InComp, FName InBoneName, bool bInFollowComponent)
{
	if (ensure(InComp))
	{
		Name = InName;
		Component = InComp;
		BoneName = InBoneName;
		bFollowComponent = bInFollowComponent;

		FTransform Transform = FTransform::Identity;
		if (BoneName != NAME_None)
		{
			Transform = FMotionWarpingTarget::GetTargetTransformFromComponent(InComp, InBoneName);
		}
		else
		{
			Transform = InComp->GetComponentTransform();
		}

		Location = Transform.GetLocation();
		Rotation = Transform.Rotator();
	}
}

FTransform FMotionWarpingTarget::GetTargetTransformFromComponent(const USceneComponent* Comp, const FName& BoneName)
{
	if (Comp == nullptr)
	{
		UE_LOG(LogMotionWarping, Warning, TEXT("FMotionWarpingTarget::GetTargetTransformFromComponent: Invalid Component"));
		return FTransform::Identity;
	}

	if (Comp->DoesSocketExist(BoneName) == false)
	{
		UE_LOG(LogMotionWarping, Warning, TEXT("FMotionWarpingTarget::GetTargetTransformFromComponent: Invalid Bone or Socket. Comp: %s Owner: %s BoneName: %s"),
			*GetNameSafe(Comp), *GetNameSafe(Comp->GetOwner()), *BoneName.ToString());

		return Comp->GetComponentTransform();
	}

	return Comp->GetSocketTransform(BoneName);
}

FTransform FMotionWarpingTarget::GetTargetTrasform() const
{
	if (Component.IsValid() && bFollowComponent)
	{
		if (BoneName != NAME_None)
		{
			return FMotionWarpingTarget::GetTargetTransformFromComponent(Component.Get(), BoneName);
		}
		else
		{
			return Component->GetComponentTransform();
		}
	}

	return FTransform(Rotation, Location);
}

// URootMotionModifier
///////////////////////////////////////////////////////////////

URootMotionModifier::URootMotionModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMotionWarpingComponent* URootMotionModifier::GetOwnerComponent() const
{
	return Cast<UMotionWarpingComponent>(GetOuter());
}

ACharacter* URootMotionModifier::GetCharacterOwner() const
{
	UMotionWarpingComponent* OwnerComp = GetOwnerComponent();
	return OwnerComp ? OwnerComp->GetCharacterOwner() : nullptr;
}

void URootMotionModifier::Update(const FMotionWarpingUpdateContext& Context)
{
	const ACharacter* CharacterOwner = GetCharacterOwner();
	if (CharacterOwner == nullptr)
	{
		return;
	}

	// Mark for removal if our animation is not relevant anymore
	if (!Context.Animation.IsValid() || Context.Animation.Get() != Animation)
	{
		UE_LOG(LogMotionWarping, Verbose, TEXT("MotionWarping: Marking RootMotionModifier for removal. Reason: Animation is not valid. Char: %s Current Animation: %s. Window: Animation: %s [%f %f] [%f %f]"),
			*GetNameSafe(CharacterOwner), *GetNameSafe(Context.Animation.Get()), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

		SetState(ERootMotionModifierState::MarkedForRemoval);
		return;
	}

	// Update playback times and weight
	PreviousPosition = Context.PreviousPosition;
	CurrentPosition = Context.CurrentPosition;
	Weight = Context.Weight;

	// Mark for removal if the animation already passed the warping window
	if (PreviousPosition >= EndTime)
	{
		UE_LOG(LogMotionWarping, Verbose, TEXT("MotionWarping: Marking RootMotionModifier for removal. Reason: Window has ended. Char: %s Animation: %s [%f %f] [%f %f]"),
			*GetNameSafe(CharacterOwner), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

		SetState(ERootMotionModifierState::MarkedForRemoval);
		return;
	}

	// Mark for removal if we jumped to a position outside the warping window
	if (State == ERootMotionModifierState::Active && PreviousPosition < EndTime && (CurrentPosition > EndTime || CurrentPosition < StartTime))
	{
		const float ExpectedDelta = Context.DeltaSeconds * Context.PlayRate;
		const float ActualDelta = CurrentPosition - PreviousPosition;
		if (!FMath::IsNearlyZero(FMath::Abs(ActualDelta - ExpectedDelta), KINDA_SMALL_NUMBER))
		{
			UE_LOG(LogMotionWarping, Verbose, TEXT("MotionWarping: Marking RootMotionModifier for removal. Reason: CurrentPosition manually changed. PrevPos: %f CurrPos: %f DeltaTime: %f ExpectedDelta: %f ActualDelta: %f"),
				PreviousPosition, CurrentPosition, Context.DeltaSeconds, ExpectedDelta, ActualDelta);

			SetState(ERootMotionModifierState::MarkedForRemoval);
			return;
		}
	}

	// Check if we are inside the warping window
	if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
	{
		// If we were waiting, switch to active
		if (GetState() == ERootMotionModifierState::Waiting)
		{
			SetState(ERootMotionModifierState::Active);
		}
	}

	if (State == ERootMotionModifierState::Active)
	{
		if (UMotionWarpingComponent* OwnerComp = GetOwnerComponent())
		{
			OnUpdateDelegate.ExecuteIfBound(OwnerComp, this);
		}
	}
}

void URootMotionModifier::SetState(ERootMotionModifierState NewState)
{
	if (State != NewState)
	{
		ERootMotionModifierState LastState = State;

		State = NewState;

		OnStateChanged(LastState);
	}
}

void URootMotionModifier::OnStateChanged(ERootMotionModifierState LastState)
{
	if (UMotionWarpingComponent* OwnerComp = GetOwnerComponent())
	{
		if (LastState != ERootMotionModifierState::Active && State == ERootMotionModifierState::Active)
		{
			const ACharacter* CharacterOwner = OwnerComp->GetCharacterOwner();
			check(CharacterOwner);
			
			ActualStartTime = PreviousPosition;

			const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FQuat CurrentRotation = CharacterOwner->GetActorQuat();
			const FVector CurrentLocation = (CharacterOwner->GetActorLocation() - CurrentRotation.GetUpVector() * CapsuleHalfHeight);
			StartTransform = FTransform(CurrentRotation, CurrentLocation);

			OnActivateDelegate.ExecuteIfBound(OwnerComp, this);
		}
		else if (LastState == ERootMotionModifierState::Active && (State == ERootMotionModifierState::Disabled || State == ERootMotionModifierState::MarkedForRemoval))
		{
			OnDeactivateDelegate.ExecuteIfBound(OwnerComp, this);
		}
	}
}

URootMotionModifier_Warp::URootMotionModifier_Warp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URootMotionModifier_Warp::Update(const FMotionWarpingUpdateContext& Context)
{
	// Update playback times and state
	Super::Update(Context);

	// Cache sync point transform and trigger OnTargetTransformChanged if needed
	const UMotionWarpingComponent* OwnerComp = GetOwnerComponent();
	if (OwnerComp && GetState() == ERootMotionModifierState::Active)
	{
		const FMotionWarpingTarget* WarpTargetPtr = OwnerComp->FindWarpTarget(WarpTargetName);

		// Disable if there is no target for us
		if (WarpTargetPtr == nullptr)
		{
			UE_LOG(LogMotionWarping, Verbose, TEXT("MotionWarping: Marking RootMotionModifier as Disabled. Reason: Invalid Warp Target (%s). Char: %s Animation: %s [%f %f] [%f %f]"),
				*WarpTargetName.ToString(), *GetNameSafe(OwnerComp->GetOwner()), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

			SetState(ERootMotionModifierState::Disabled);
			return;
		}

		// Get the warp point sent by the game
		FTransform WarpPointTransformGame = WarpTargetPtr->GetTargetTrasform();

		// Initialize our target transform (where the root should end at the end of the window) with the warp point sent by the game
		FTransform TargetTransform = WarpPointTransformGame;

		// Check if a warp point is defined in the animation. If so, we need to extract it and offset the target transform 
		// the same amount the root bone is offset from the warp point in the animation
		if (WarpPointAnimProvider != EWarpPointAnimProvider::None)
		{
			if (!CachedOffsetFromWarpPoint.IsSet())
			{
				if (const ACharacter* CharacterOwner = GetCharacterOwner())
				{
					if (WarpPointAnimProvider == EWarpPointAnimProvider::Static)
					{
						CachedOffsetFromWarpPoint = UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(*CharacterOwner, GetAnimation(), EndTime, WarpPointAnimTransform);
					}
					else if (WarpPointAnimProvider == EWarpPointAnimProvider::Bone)
					{
						CachedOffsetFromWarpPoint = UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime(*CharacterOwner, GetAnimation(), EndTime, WarpPointAnimBoneName);
					}
				}
			}

			// Update Target Transform based on the offset between the root and the warp point in the animation
			TargetTransform = CachedOffsetFromWarpPoint.GetValue() * WarpPointTransformGame;
		}

		if (!CachedTargetTransform.Equals(TargetTransform))
		{
			CachedTargetTransform = TargetTransform;

			OnTargetTransformChanged();
		}
	}
}

void URootMotionModifier_Warp::OnTargetTransformChanged()
{
	if (const ACharacter* CharacterOwner = GetCharacterOwner())
	{
		ActualStartTime = PreviousPosition;

		const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FQuat CurrentRotation = CharacterOwner->GetActorQuat();
		const FVector CurrentLocation = (CharacterOwner->GetActorLocation() - CurrentRotation.GetUpVector() * CapsuleHalfHeight);
		StartTransform = FTransform(CurrentRotation, CurrentLocation);
	}
}

FQuat URootMotionModifier_Warp::GetTargetRotation() const
{
	if (RotationType == EMotionWarpRotationType::Default)
	{
		return CachedTargetTransform.GetRotation();
	}
	else if (RotationType == EMotionWarpRotationType::Facing)
	{
		if (const ACharacter* CharacterOwner = GetCharacterOwner())
		{
			const FTransform& CharacterTransform = CharacterOwner->GetActorTransform();
			const FVector ToSyncPoint = (CachedTargetTransform.GetLocation() - CharacterTransform.GetLocation()).GetSafeNormal2D();
			return FRotationMatrix::MakeFromXZ(ToSyncPoint, FVector::UpVector).ToQuat();
		}
	}

	return FQuat::Identity;
}

FQuat URootMotionModifier_Warp::WarpRotation(const FTransform& RootMotionDelta, const FTransform& RootMotionTotal, float DeltaSeconds)
{
	const ACharacter* CharacterOwner = GetCharacterOwner();
	if (CharacterOwner == nullptr)
	{
		return FQuat::Identity;
	}

	const FQuat TotalRootMotionRotation = RootMotionTotal.GetRotation();
	const FQuat CurrentRotation = CharacterOwner->GetActorQuat() * CharacterOwner->GetBaseRotationOffset();
	const FQuat TargetRotation = CurrentRotation.Inverse() * (GetTargetRotation() * CharacterOwner->GetBaseRotationOffset());
	const float TimeRemaining = (EndTime - PreviousPosition) * WarpRotationTimeMultiplier;
	const float Alpha = FMath::Clamp(DeltaSeconds / TimeRemaining, 0.f, 1.f);
	const FQuat TargetRotThisFrame = FQuat::Slerp(TotalRootMotionRotation, TargetRotation, Alpha);
	const FQuat DeltaOut = TargetRotThisFrame * TotalRootMotionRotation.Inverse();
	return (DeltaOut * RootMotionDelta.GetRotation());
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void URootMotionModifier_Warp::PrintLog(const FString& Name, const FTransform& OriginalRootMotion, const FTransform& WarpedRootMotion) const
{
	if (const ACharacter* CharacterOwner = GetCharacterOwner())
	{
		const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector CurrentLocation = (CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CapsuleHalfHeight));
		const FVector CurrentToTarget = (GetTargetLocation() - CurrentLocation).GetSafeNormal2D();
		const FVector FutureLocation = CurrentLocation + (CharacterOwner->GetMesh()->ConvertLocalRootMotionToWorld(WarpedRootMotion)).GetTranslation();
		const FRotator CurrentRotation = CharacterOwner->GetActorRotation();
		const FRotator FutureRotation = (WarpedRootMotion.GetRotation() * CharacterOwner->GetActorQuat()).Rotator();
		const float Dot = FVector::DotProduct(CharacterOwner->GetActorForwardVector(), CurrentToTarget);
		const float CurrentDist2D = FVector::Dist2D(GetTargetLocation(), CurrentLocation);
		const float FutureDist2D = FVector::Dist2D(GetTargetLocation(), FutureLocation);
		const float DeltaSeconds = CharacterOwner->GetWorld()->GetDeltaSeconds();
		const float Speed = WarpedRootMotion.GetTranslation().Size() / DeltaSeconds;
		const float EndTimeOffset = CurrentPosition - EndTime;

		UE_LOG(LogMotionWarping, Log, TEXT("%s NetMode: %d Char: %s Anim: %s Win: [%f %f][%f %f] DT: %f WT: %f ETOffset: %f Dist2D: %f Z: %f FDist2D: %f FZ: %f Dot: %f Delta: %s (%f) FDelta: %s (%f) Speed: %f Loc: %s FLoc: %s Rot: %s FRot: %s"),
			*Name, (int32)CharacterOwner->GetWorld()->GetNetMode(), *GetNameSafe(CharacterOwner), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition, DeltaSeconds, CharacterOwner->GetWorld()->GetTimeSeconds(), EndTimeOffset,
			CurrentDist2D, (GetTargetLocation().Z - CurrentLocation.Z), FutureDist2D, (GetTargetLocation().Z - FutureLocation.Z), Dot,
			*OriginalRootMotion.GetTranslation().ToString(), OriginalRootMotion.GetTranslation().Size(), *WarpedRootMotion.GetTranslation().ToString(), WarpedRootMotion.GetTranslation().Size(), Speed,
			*CurrentLocation.ToString(), *FutureLocation.ToString(), *CurrentRotation.ToCompactString(), *FutureRotation.ToCompactString());
	}
}
#endif

// URootMotionModifier_SimpleWarp
///////////////////////////////////////////////////////////////

UDEPRECATED_RootMotionModifier_SimpleWarp::UDEPRECATED_RootMotionModifier_SimpleWarp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FTransform UDEPRECATED_RootMotionModifier_SimpleWarp::ProcessRootMotion(const FTransform& InRootMotion, float DeltaSeconds)
{
	const ACharacter* CharacterOwner = GetCharacterOwner();
	if (CharacterOwner == nullptr)
	{
		return InRootMotion;
	}

	const FTransform& CharacterTransform = CharacterOwner->GetActorTransform();

	FTransform FinalRootMotion = InRootMotion;

	const FTransform RootMotionTotal = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Animation.Get(), PreviousPosition, EndTime);

	if (bWarpTranslation)
	{
		FVector DeltaTranslation = InRootMotion.GetTranslation();

		const FTransform RootMotionDelta = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(Animation.Get(), PreviousPosition, FMath::Min(CurrentPosition, EndTime));

		const float HorizontalDelta = RootMotionDelta.GetTranslation().Size2D();
		const float HorizontalTarget = FVector::Dist2D(CharacterTransform.GetLocation(), GetTargetLocation());
		const float HorizontalOriginal = RootMotionTotal.GetTranslation().Size2D();
		const float HorizontalTranslationWarped = !FMath::IsNearlyZero(HorizontalOriginal) ? ((HorizontalDelta * HorizontalTarget) / HorizontalOriginal) : 0.f;

		const FTransform MeshRelativeTransform = FTransform(CharacterOwner->GetBaseRotationOffset(), CharacterOwner->GetBaseTranslationOffset());
		const FTransform MeshTransform = MeshRelativeTransform * CharacterOwner->GetActorTransform();
		DeltaTranslation = MeshTransform.InverseTransformPositionNoScale(GetTargetLocation()).GetSafeNormal2D() * HorizontalTranslationWarped;

		if (!bIgnoreZAxis)
		{
			const float CapsuleHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector CapsuleBottomLocation = (CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CapsuleHalfHeight));
			const float VerticalDelta = RootMotionDelta.GetTranslation().Z;
			const float VerticalTarget = GetTargetLocation().Z - CapsuleBottomLocation.Z;
			const float VerticalOriginal = RootMotionTotal.GetTranslation().Z;
			const float VerticalTranslationWarped = !FMath::IsNearlyZero(VerticalOriginal) ? ((VerticalDelta * VerticalTarget) / VerticalOriginal) : 0.f;

			DeltaTranslation.Z = VerticalTranslationWarped;
		}
		else
		{
			DeltaTranslation.Z = InRootMotion.GetTranslation().Z;
		}

		FinalRootMotion.SetTranslation(DeltaTranslation);
	}

	if (bWarpRotation)
	{
		const FQuat WarpedRotation = WarpRotation(InRootMotion, RootMotionTotal, DeltaSeconds);
		FinalRootMotion.SetRotation(WarpedRotation);
	}

	// Debug
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 DebugLevel = FMotionWarpingCVars::CVarMotionWarpingDebug.GetValueOnGameThread();
	if (DebugLevel == 1 || DebugLevel == 3)
	{
		PrintLog(TEXT("SimpleWarp"), InRootMotion, FinalRootMotion);
	}

	if (DebugLevel == 2 || DebugLevel == 3)
	{
		const float DrawDebugDuration = FMotionWarpingCVars::CVarMotionWarpingDrawDebugDuration.GetValueOnGameThread();
		DrawDebugCoordinateSystem(CharacterOwner->GetWorld(), GetTargetLocation(), GetTargetRotator(), 50.f, false, DrawDebugDuration, 0, 1.f);
	}
#endif

	return FinalRootMotion;
}

// URootMotionModifier_Scale
///////////////////////////////////////////////////////////////

URootMotionModifier_Scale* URootMotionModifier_Scale::AddRootMotionModifierScale(UMotionWarpingComponent* InMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FVector InScale)
{
	if (ensureAlways(InMotionWarpingComp))
	{
		URootMotionModifier_Scale* NewModifier = NewObject<URootMotionModifier_Scale>(InMotionWarpingComp);
		NewModifier->Animation = InAnimation;
		NewModifier->StartTime = InStartTime;
		NewModifier->EndTime = InEndTime;
		NewModifier->Scale = InScale;

		InMotionWarpingComp->AddModifier(NewModifier);

		return NewModifier;
	}

	return nullptr;
}
