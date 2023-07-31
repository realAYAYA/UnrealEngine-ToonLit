// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraMovementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetMathLibrary.h"

LLM_DEFINE_TAG(VirtualCamera_VirtualCameraMovementComponent);

namespace
{
	float ClampStabilizationScale(float StabilizationScale)
	{
		const float StabilizationMin = 0.0f;
		const float StabilizationMax = 0.97f;
		const float StabilizationExp = 0.25f;

		// Stabilization is applied as an exponential curve defined by StabilizationMax
		// This exponent is less than 1 so value changes at numbers closer to 0 have a greater degree of change than those closer to 1.
		// This has the effect of stabilization being introduced much faster when it is first applied, and giving a finer degree of control as values increase.
		// This is necessary because stabilization is not noticeable or useful until a certain amount is applied.

		float NewStabilizationAmountRotationX = FMath::Clamp<float>(StabilizationScale, StabilizationMin, StabilizationMax);
		return FMath::Pow(NewStabilizationAmountRotationX, StabilizationExp) * StabilizationMax;
	}
}

UVirtualCameraMovementComponent::UVirtualCameraMovementComponent(const FObjectInitializer& ObjectInitializer)
{
	LLM_SCOPE_BYTAG(VirtualCamera_VirtualCameraMovementComponent);

	// Initialize Axis Settings Map
	AxisSettings.Add(EVirtualCameraAxis::LocationX);
	AxisSettings.Add(EVirtualCameraAxis::LocationY);
	AxisSettings.Add(EVirtualCameraAxis::LocationZ);
	AxisSettings.Add(EVirtualCameraAxis::RotationX);
	AxisSettings.Add(EVirtualCameraAxis::RotationY);
	AxisSettings.Add(EVirtualCameraAxis::RotationZ);

	bLockRelativeToFirstLockAxis = true;
	bUseGlobalBoom = true;
	bZeroDutchOnLock = true;
	CachedLockingAxis = FQuat::Identity;
}

void UVirtualCameraMovementComponent::AddInputVector(FVector WorldVector, bool bForce /*=false*/)
{
	if (WorldVector.IsZero())
	{
		return;
	}

	ApplyLocationScaling(WorldVector);
	ApplyLocationLocks(WorldVector);

	TargetLocation += WorldVector;
}

void UVirtualCameraMovementComponent::AddInputVectorFromController(FVector WorldVector, EVirtualCameraAxis MovementScaleAxis)
{
	if (WorldVector.IsZero())
	{
		return;
	}

	WorldVector *= AxisSettings[MovementScaleAxis].MovementScale;

	// For Controller movement, move the RootUpdatedComponent
	FromControllerTargetLocation += WorldVector;
}

void UVirtualCameraMovementComponent::ProcessMovementInput(const FVector& TrackerLocation, const FRotator& TrackerRotation)
{
	FVector DeltaMovement = TrackerLocation - PreviousTrackerLocation;
	FRotator RotationReorient = GetRotationOffset();
	RotationReorient.Pitch = 0.f;
	// make sure movement is correctly oriented to current direction camera is facing
	DeltaMovement = RotationReorient.UnrotateVector(DeltaMovement);
	AddInputVector(DeltaMovement);
	PreviousTrackerLocation = TrackerLocation;

	// Calculate the global change in rotation and apply it
	FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(TrackerRotation, PreviousTrackerRotation);
	ApplyRotationLocks(DeltaRotation);
	TargetRotation = TrackerRotation - GetRotationOffset() + GetOwner()->GetActorRotation();
	PreviousTrackerRotation = TrackerRotation;

	{
		FHitResult OutResult = FHitResult();
		SafeMoveUpdatedComponent(GetStabilizedDeltaLocation(), GetStabilizedRotation(), false, OutResult);
	}

	// Apply the location change for the RootUpdatedComponent
	if (RootUpdatedComponent)
	{
		TGuardValue<decltype(UpdatedComponent)> UpdateComponentGuard(UpdatedComponent, RootUpdatedComponent);
		TGuardValue<decltype(UpdatedPrimitive)> UpdatePrimitiveGuard(UpdatedPrimitive, RootUpdatedPrimitive);
		FHitResult OutResult = FHitResult();
		SafeMoveUpdatedComponent(FromControllerTargetLocation, FQuat::Identity, false, OutResult);
		FromControllerTargetLocation = FVector::ZeroVector;
	}
}

bool UVirtualCameraMovementComponent::ToggleAxisLock(const EVirtualCameraAxis AxisToToggle)
{
	const bool WasLocked = IsLocationLockingActive();
	const bool bIsLocked = AxisSettings[AxisToToggle].ToggleLock();

	if (AxisToToggle == EVirtualCameraAxis::RotationX && bZeroDutchOnLock && bIsLocked)
	{
		// This needs to be applied to the lock offset directly to avoid adding to freeze
		AxisSettings[EVirtualCameraAxis::RotationX].LockRotationOffset = TargetRotation.Roll;
	}

	// Update cached locking axis if coming from completely unlocked state
	if (bLockRelativeToFirstLockAxis && !WasLocked)
	{
		CachedLockingAxis = UpdatedComponent->GetComponentRotation().Quaternion();
	}

	return bIsLocked;
}

bool UVirtualCameraMovementComponent::ToggleAxisFreeze(const EVirtualCameraAxis AxisToToggle)
{
	// Leaving this out of header file for the inevitable refactor that will cause this to be more that one line of code.
	return AxisSettings[AxisToToggle].bIsFrozen = !AxisSettings[AxisToToggle].bIsFrozen;
}

float UVirtualCameraMovementComponent::SetAxisStabilizationScale(const EVirtualCameraAxis AxisToAdjust, float NewStabilizationAmount)
{
	AxisSettings[AxisToAdjust].StabilizationScale = FMath::Clamp<float>(NewStabilizationAmount, 0.0f, 1.0f);
	return AxisSettings[AxisToAdjust].StabilizationScale;
}

float UVirtualCameraMovementComponent::GetAxisStabilizationScale(const EVirtualCameraAxis AxisToRetrieve) const
{
	return AxisSettings[AxisToRetrieve].StabilizationScale;
}

void UVirtualCameraMovementComponent::ResetCameraOffsetsToTracker()
{
	// Reorient pawn to be in "stage space", with stage origin acting as pawn root component
	TargetLocation = PreviousTrackerLocation;
	TargetRotation = PreviousTrackerRotation;
	UpdatedComponent->SetRelativeLocation(TargetLocation);
	UpdatedComponent->SetRelativeRotation(TargetRotation);

	// Clear all locks
	for (auto Iterator = AxisSettings.CreateIterator(); Iterator; ++Iterator)
	{
		Iterator.Value().SetIsLocked(false);
		Iterator.Value().bIsFrozen = false;
		Iterator.Value().FreezeRotationOffset = 0.f;
		Iterator.Value().FreezeLocationOffset = FVector::ZeroVector;
	}

	OnOffsetReset.Broadcast();
}

bool UVirtualCameraMovementComponent::IsLocationLockingActive() const 
{
	return AxisSettings[EVirtualCameraAxis::LocationX].bIsLocked || AxisSettings[EVirtualCameraAxis::LocationY].bIsLocked || AxisSettings[EVirtualCameraAxis::LocationZ].bIsLocked;
}

bool UVirtualCameraMovementComponent::IsAxisLocked(const EVirtualCameraAxis AxisToCheck) const
{
	return AxisSettings[AxisToCheck].bIsLocked;
}

void UVirtualCameraMovementComponent::OnMoveForward(const float InValue)
{
	FVector InputVector = UpdatedComponent->GetForwardVector();
	InputVector = GetOwner()->GetActorRotation().UnrotateVector(InputVector);
	AddInputVectorFromController(InputVector * InValue, EVirtualCameraAxis::LocationX);
}

void UVirtualCameraMovementComponent::OnMoveRight(const float InValue)
{
	FVector InputVector = UpdatedComponent->GetRightVector();
	InputVector = GetOwner()->GetActorRotation().UnrotateVector(InputVector);
	AddInputVectorFromController(InputVector * InValue, EVirtualCameraAxis::LocationY);
}

void UVirtualCameraMovementComponent::OnMoveUp(const float InValue)
{
	if (bUseGlobalBoom)
	{
		FVector InputVector = FVector(0.f, 0.f, 1.f);
		AddInputVectorFromController(InputVector * InValue, EVirtualCameraAxis::LocationZ);
	}
	else
	{
		FVector InputVector = UpdatedComponent->GetUpVector();
		InputVector = GetOwner()->GetActorRotation().UnrotateVector(InputVector);
		AddInputVectorFromController(InputVector * InValue, EVirtualCameraAxis::LocationZ);
	}
}

void UVirtualCameraMovementComponent::Teleport(const FTransform& TargetTransform)
{
	ResetCameraOffsetsToTracker();

	// The change between where we were and where we are teleporting to.
	FVector DeltaOffset = TargetTransform.GetLocation() - UpdatedComponent->GetComponentLocation();
	DeltaOffset = GetOwner()->GetActorRotation().UnrotateVector(DeltaOffset);

	UpdatedComponent->AddLocalOffset(DeltaOffset);
	TargetLocation += DeltaOffset;

	if (RootUpdatedComponent)
	{
		RootUpdatedComponent->SetRelativeLocation(FVector::ZeroVector);
		FromControllerTargetLocation = FVector::ZeroVector;
	}
}

void UVirtualCameraMovementComponent::SetRootComponent(USceneComponent* InFromController)
{
	RootUpdatedComponent = InFromController;
	RootUpdatedPrimitive = Cast<UPrimitiveComponent>(RootUpdatedComponent);
}

void UVirtualCameraMovementComponent::ApplyLocationLocks(const FVector& InVector)
{
	FVector ForwardVector;
	FVector RightVector;
	FVector UpVector;

	GetDirectionVectorsForCamera(ForwardVector, RightVector, UpVector, true);

	FVector AdjustedVectorX = InVector - FVector::VectorPlaneProject(InVector, ForwardVector);
	FVector AdjustedVectorY = InVector - FVector::VectorPlaneProject(InVector, RightVector);
	FVector AdjustedVectorZ = InVector - FVector::VectorPlaneProject(InVector, UpVector);

	if (AxisSettings[EVirtualCameraAxis::LocationX].IsAxisImmobilized())
	{
		AxisSettings[EVirtualCameraAxis::LocationX].AddLocationOffset(AdjustedVectorX);
	}

	if (AxisSettings[EVirtualCameraAxis::LocationY].IsAxisImmobilized())
	{
		AxisSettings[EVirtualCameraAxis::LocationY].AddLocationOffset(AdjustedVectorY);
	}

	if (AxisSettings[EVirtualCameraAxis::LocationZ].IsAxisImmobilized())
	{
		AxisSettings[EVirtualCameraAxis::LocationZ].AddLocationOffset(AdjustedVectorZ);
	}
}

FVector UVirtualCameraMovementComponent::GetStabilizedDeltaLocation() const 
{
	FVector ReturnVector = FVector::ZeroVector;
	FVector ClosestPoint;

	FVector TargetLocationWithOffsets = TargetLocation - GetLocationOffset();

	const FVector UpdatedComponentRelativeLocation = UpdatedComponent->GetRelativeLocation();

	// Calculate each component by getting the needed vector component and doing a lerp with the stabilization scale for that axis
	FMath::PointDistToLine(TargetLocationWithOffsets, UpdatedComponent->GetForwardVector(), UpdatedComponentRelativeLocation, ClosestPoint);
	ReturnVector += (ClosestPoint - UpdatedComponentRelativeLocation) * (1 - ClampStabilizationScale(AxisSettings[EVirtualCameraAxis::LocationX].StabilizationScale));

	FMath::PointDistToLine(TargetLocationWithOffsets, UpdatedComponent->GetRightVector(), UpdatedComponentRelativeLocation, ClosestPoint);
	ReturnVector += (ClosestPoint - UpdatedComponentRelativeLocation) * (1 - ClampStabilizationScale(AxisSettings[EVirtualCameraAxis::LocationY].StabilizationScale));

	FMath::PointDistToLine(TargetLocationWithOffsets, UpdatedComponent->GetUpVector(), UpdatedComponentRelativeLocation, ClosestPoint);
	ReturnVector += (ClosestPoint - UpdatedComponentRelativeLocation) * (1 - ClampStabilizationScale(AxisSettings[EVirtualCameraAxis::LocationZ].StabilizationScale));

	ReturnVector = GetOwner()->GetActorRotation().RotateVector(ReturnVector);
	return ReturnVector;
}

void UVirtualCameraMovementComponent::ApplyLocationScaling(FVector& VectorToAdjust)
{
	// Get the axes to scale along
	FVector ForwardVector;
	FVector RightVector;
	FVector UpVector;

	GetDirectionVectorsForCamera(ForwardVector, RightVector, UpVector, false);

	// Orient to global Z up, but maintain yaw
	ForwardVector = FVector::VectorPlaneProject(ForwardVector, FVector::UpVector);
	RightVector = FVector::VectorPlaneProject(RightVector, FVector::UpVector);
	UpVector = FVector::UpVector;

	ForwardVector.Normalize();
	RightVector.Normalize();

	FVector XComponent = VectorToAdjust.ProjectOnTo(ForwardVector);
	FVector YComponent = VectorToAdjust.ProjectOnTo(RightVector);
	FVector ZComponent = VectorToAdjust.ProjectOnTo(UpVector);

	VectorToAdjust = XComponent * AxisSettings[EVirtualCameraAxis::LocationX].MovementScale;
	VectorToAdjust += YComponent * AxisSettings[EVirtualCameraAxis::LocationY].MovementScale;
	VectorToAdjust += ZComponent * AxisSettings[EVirtualCameraAxis::LocationZ].MovementScale;
}

void UVirtualCameraMovementComponent::ApplyRotationLocks(const FRotator& InRotation)
{
	// The AddOffset function will ignore incoming offsets if the axis is not immobilized
	AxisSettings[EVirtualCameraAxis::RotationX].AddRotationOffset(InRotation.Roll);
	AxisSettings[EVirtualCameraAxis::RotationY].AddRotationOffset(InRotation.Pitch);
	AxisSettings[EVirtualCameraAxis::RotationZ].AddRotationOffset(InRotation.Yaw);
}

FRotator UVirtualCameraMovementComponent::GetStabilizedRotation() const
{
	// Needs to be done as Quaternions to avoid gimbal locking
	FRotator TargetAdjustment = UKismetMathLibrary::ComposeRotators(TargetRotation, UpdatedComponent->GetComponentRotation().GetInverse());

	TargetAdjustment.Roll  *= (1 - ClampStabilizationScale(AxisSettings[EVirtualCameraAxis::RotationX].StabilizationScale));
	TargetAdjustment.Pitch *= (1 - ClampStabilizationScale(AxisSettings[EVirtualCameraAxis::RotationY].StabilizationScale));
	TargetAdjustment.Yaw   *= (1 - ClampStabilizationScale(AxisSettings[EVirtualCameraAxis::RotationZ].StabilizationScale));

	return UKismetMathLibrary::ComposeRotators(TargetAdjustment, UpdatedComponent->GetComponentRotation());
}

void UVirtualCameraMovementComponent::GetDirectionVectorsForCamera(FVector& OutForwardVector, FVector& OutRightVector, FVector& OutUpVector, bool bTryLock) const
{
	// Break the vector into local components so we can apply individual scaling
	if (bTryLock && bLockRelativeToFirstLockAxis && IsLocationLockingActive())
	{
		// If needed, use the cached axis rather than always using local
		OutForwardVector = CachedLockingAxis.GetForwardVector();
		OutRightVector = CachedLockingAxis.GetRightVector();
		OutUpVector = CachedLockingAxis.GetUpVector();
	}
	else
	{
		OutForwardVector = UpdatedComponent->GetForwardVector();
		OutRightVector = UpdatedComponent->GetRightVector();
		OutUpVector = UpdatedComponent->GetUpVector();
	}

	OutForwardVector = GetOwner()->GetActorRotation().UnrotateVector(OutForwardVector);
	OutRightVector = GetOwner()->GetActorRotation().UnrotateVector(OutRightVector);
	OutUpVector = GetOwner()->GetActorRotation().UnrotateVector(OutUpVector);
}

FVector UVirtualCameraMovementComponent::GetLocationOffset() const 
{
	return AxisSettings[EVirtualCameraAxis::LocationX].GetLocationOffset()
		+ AxisSettings[EVirtualCameraAxis::LocationY].GetLocationOffset()
		+ AxisSettings[EVirtualCameraAxis::LocationZ].GetLocationOffset();
}

FRotator UVirtualCameraMovementComponent::GetRotationOffset() const 
{
	return FRotator(
		AxisSettings[EVirtualCameraAxis::RotationY].GetRotationOffset(),
		AxisSettings[EVirtualCameraAxis::RotationZ].GetRotationOffset(),
		AxisSettings[EVirtualCameraAxis::RotationX].GetRotationOffset()
	);
}
