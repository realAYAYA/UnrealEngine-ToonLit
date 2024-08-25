// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "Components/PrimitiveComponent.h"
#include "MoverComponent.h"
#include "MoverLog.h"
#include "Kismet/KismetMathLibrary.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BasedMovementUtils)

void FRelativeBaseInfo::Clear()
{
	MovementBase = nullptr;
	BoneName = NAME_None;
	Location = FVector::ZeroVector;
	Rotation = FQuat::Identity;
	ContactLocalPosition = FVector::ZeroVector;
}

bool FRelativeBaseInfo::HasRelativeInfo() const
{
	return MovementBase != nullptr;
}

bool FRelativeBaseInfo::UsesSameBase(const FRelativeBaseInfo& Other) const
{
	return UsesSameBase(Other.MovementBase.Get(), Other.BoneName);
}

bool FRelativeBaseInfo::UsesSameBase(const UPrimitiveComponent* OtherComp, FName OtherBoneName) const
{
	return HasRelativeInfo()
		&& (MovementBase == OtherComp)
		&& (BoneName == OtherBoneName);
}

void FRelativeBaseInfo::SetFromFloorResult(const FFloorCheckResult& FloorTestResult)
{
	bool bDidSucceed = false;

	if (FloorTestResult.bWalkableFloor)
	{
		MovementBase = FloorTestResult.HitResult.GetComponent();

		if (MovementBase.IsValid())
		{
			BoneName = FloorTestResult.HitResult.BoneName;

			if (UBasedMovementUtils::GetMovementBaseTransform(MovementBase.Get(), BoneName, OUT Location, OUT Rotation) &&
				UBasedMovementUtils::TransformWorldLocationToBased(MovementBase.Get(), BoneName, FloorTestResult.HitResult.ImpactPoint, OUT ContactLocalPosition))
			{
				bDidSucceed = true;
			}
		}
	}

	if (!bDidSucceed)
	{
		Clear();
	}
}

void FRelativeBaseInfo::SetFromComponent(UPrimitiveComponent* InRelativeComp, FName InBoneName)
{
	bool bDidSucceed = false;

	MovementBase = InRelativeComp;

	if (MovementBase.IsValid())
	{
		BoneName = InBoneName;
		bDidSucceed = UBasedMovementUtils::GetMovementBaseTransform(MovementBase.Get(), BoneName, /*out*/Location, /*out*/Rotation);
	}

	if (!bDidSucceed)
	{
		Clear();
	}
}


FString FRelativeBaseInfo::ToString() const
{
	if (MovementBase.IsValid())
	{
		return FString::Printf(TEXT("Base: %s, Loc: %s, Rot: %s, LocalContact: %s"),
			*GetNameSafe(MovementBase->GetOwner()),
			*Location.ToCompactString(),
			*Rotation.Rotator().ToCompactString(),
			*ContactLocalPosition.ToCompactString());
	}

	return FString(TEXT("Base: NULL"));
}

bool UBasedMovementUtils::IsADynamicBase(const UPrimitiveComponent* MovementBase)
{
	return (MovementBase && MovementBase->Mobility == EComponentMobility::Movable);
}

bool UBasedMovementUtils::IsBaseSimulatingPhysics(const UPrimitiveComponent* MovementBase)
{
	bool bBaseIsSimulatingPhysics = false;
	const USceneComponent* AttachParent = MovementBase;
	while (!bBaseIsSimulatingPhysics && AttachParent)
	{
		bBaseIsSimulatingPhysics = AttachParent->IsSimulatingPhysics();
		AttachParent = AttachParent->GetAttachParent();
	}
	return bBaseIsSimulatingPhysics;
}


bool UBasedMovementUtils::GetMovementBaseTransform(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector& OutLocation, FQuat& OutQuat)
{
	if (MovementBase)
	{
		bool bBoneNameIsInvalid = false;

		if (BoneName != NAME_None)
		{
			// Check if this socket or bone exists (DoesSocketExist checks for either, as does requesting the transform).
			if (MovementBase->DoesSocketExist(BoneName))
			{
				MovementBase->GetSocketWorldLocationAndRotation(BoneName, OutLocation, OutQuat);
				return true;
			}

			bBoneNameIsInvalid = true;
			UE_LOG(LogMover, Warning, TEXT("GetMovementBaseTransform(): Invalid bone or socket '%s' for PrimitiveComponent base %s. Using component's root transform instead."), *BoneName.ToString(), *GetPathNameSafe(MovementBase));
		}

		OutLocation = MovementBase->GetComponentLocation();
		OutQuat = MovementBase->GetComponentQuat();
		return !bBoneNameIsInvalid;
	}

	// nullptr MovementBase
	OutLocation = FVector::ZeroVector;
	OutQuat = FQuat::Identity;
	return false;
}


bool UBasedMovementUtils::TransformBasedLocationToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalLocation, FVector& OutLocationWorldSpace)
{
	FVector BaseLocation;
	FQuat BaseQuat;
	
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ BaseLocation, /*out*/ BaseQuat))
	{ 
		TransformLocationToWorld(BaseLocation, BaseQuat, LocalLocation, OutLocationWorldSpace);
		return true;
	}
	
	return false;
}


bool UBasedMovementUtils::TransformWorldLocationToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceLocation, FVector& OutLocalLocation)
{
	FVector BaseLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ BaseLocation, /*out*/ BaseQuat))
	{
		TransformLocationToLocal(BaseLocation, BaseQuat, WorldSpaceLocation, OutLocalLocation);
		return true;
	}

	return false;
}


bool UBasedMovementUtils::TransformBasedDirectionToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalDirection, FVector& OutDirectionWorldSpace)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformDirectionToWorld(BaseQuat, LocalDirection, OutDirectionWorldSpace);
		return true;
	}

	return false;
}


bool UBasedMovementUtils::TransformWorldDirectionToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceDirection, FVector& OutLocalDirection)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformDirectionToLocal(BaseQuat, WorldSpaceDirection, OutLocalDirection);
		return true;
	}

	return false;
}


bool UBasedMovementUtils::TransformBasedRotatorToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FRotator LocalRotator, FRotator& OutWorldSpaceRotator)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformRotatorToWorld(BaseQuat, LocalRotator, OutWorldSpaceRotator);
		return true;
	}

	return false;
}


bool UBasedMovementUtils::TransformWorldRotatorToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FRotator WorldSpaceRotator, FRotator& OutLocalRotator)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformRotatorToLocal(BaseQuat, WorldSpaceRotator, OutLocalRotator);
		return true;
	}
	return false;
}


void UBasedMovementUtils::TransformLocationToWorld(FVector BasePos, FQuat BaseQuat, FVector LocalLocation, FVector& OutLocationWorldSpace)
{
	OutLocationWorldSpace = FTransform(BaseQuat, BasePos).TransformPositionNoScale(LocalLocation);
}

void UBasedMovementUtils::TransformLocationToLocal(FVector BasePos, FQuat BaseQuat, FVector WorldSpaceLocation, FVector& OutLocalLocation)
{
	OutLocalLocation = FTransform(BaseQuat, BasePos).InverseTransformPositionNoScale(WorldSpaceLocation);
}

void UBasedMovementUtils::TransformDirectionToWorld(FQuat BaseQuat, FVector LocalDirection, FVector& OutDirectionWorldSpace)
{
	OutDirectionWorldSpace = BaseQuat.RotateVector(LocalDirection);
}

void UBasedMovementUtils::TransformDirectionToLocal(FQuat BaseQuat, FVector WorldSpaceDirection, FVector& OutLocalDirection)
{
	OutLocalDirection = BaseQuat.UnrotateVector(WorldSpaceDirection);
}

void UBasedMovementUtils::TransformRotatorToWorld(FQuat BaseQuat, FRotator LocalRotator, FRotator& OutWorldSpaceRotator)
{
	FQuat LocalQuat(LocalRotator);
	OutWorldSpaceRotator = (BaseQuat * LocalQuat).Rotator();
}

void UBasedMovementUtils::TransformRotatorToLocal(FQuat BaseQuat, FRotator WorldSpaceRotator, FRotator& OutLocalRotator)
{
	FQuat WorldQuat(WorldSpaceRotator);
	OutLocalRotator = (BaseQuat.Inverse() * WorldQuat).Rotator();
}

void UBasedMovementUtils::AddTickDependency(FTickFunction& BasedObjectTick, UPrimitiveComponent* NewBase)
{
	if (NewBase && IsADynamicBase(NewBase))
	{
		if (NewBase->PrimaryComponentTick.bCanEverTick)
		{
			BasedObjectTick.AddPrerequisite(NewBase, NewBase->PrimaryComponentTick);
		}

		AActor* NewBaseOwner = NewBase->GetOwner();
		if (NewBaseOwner)
		{
			if (NewBaseOwner->PrimaryActorTick.bCanEverTick)
			{
				BasedObjectTick.AddPrerequisite(NewBaseOwner, NewBaseOwner->PrimaryActorTick);
			}

			// @TODO: We need to find a more efficient way of finding all ticking components in an actor.
			for (UActorComponent* Component : NewBaseOwner->GetComponents())
			{
				// Dont allow a based component (e.g. a particle system) to push us into a different tick group
				if (Component && Component->PrimaryComponentTick.bCanEverTick && Component->PrimaryComponentTick.TickGroup <= BasedObjectTick.TickGroup)
				{
					BasedObjectTick.AddPrerequisite(Component, Component->PrimaryComponentTick);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogMover, Warning, TEXT("Attempted to AddTickDependency on an invalid or non-dynamic base: %s"), *GetNameSafe(NewBase));
	}
}

void UBasedMovementUtils::RemoveTickDependency(FTickFunction& BasedObjectTick, UPrimitiveComponent* OldBase)
{
	if (OldBase)
	{
		BasedObjectTick.RemovePrerequisite(OldBase, OldBase->PrimaryComponentTick);
		
		if (AActor* OldBaseOwner = OldBase->GetOwner())
		{
			BasedObjectTick.RemovePrerequisite(OldBaseOwner, OldBaseOwner->PrimaryActorTick);

			// @TODO: We need to find a more efficient way of finding all ticking components in an actor.
			for (UActorComponent* Component : OldBaseOwner->GetComponents())
			{
				if (Component && Component->PrimaryComponentTick.bCanEverTick)
				{
					BasedObjectTick.RemovePrerequisite(Component, Component->PrimaryComponentTick);
				}
			}
		}
	}
}


void UBasedMovementUtils::UpdateSimpleBasedMovement(UMoverComponent* TargetMoverComp)
{
	if (!TargetMoverComp)
	{
		return;
	}

	UMoverBlackboard* SimBlackboard = TargetMoverComp->GetSimBlackboard_Mutable();
	USceneComponent* UpdatedComponent = TargetMoverComp->UpdatedComponent;

	bool bIgnoreBaseRotation = false;

	if (const UCommonLegacyMovementSettings* CommonSettings = TargetMoverComp->FindSharedSettings<UCommonLegacyMovementSettings>())
	{
		bIgnoreBaseRotation = CommonSettings->bIgnoreBaseRotation;
	}

	bool bDidGetUpToDate = false;
	if (TargetMoverComp->HasValidCachedState())
	{
		FRelativeBaseInfo LastFoundBaseInfo;	// Last-found is the most recent capture during movement, likely set this sim frame
		FRelativeBaseInfo LastAppliedBaseInfo;	// Last-applied is the one that our based movement is up to date with, likely set in the last sim frame
		FRelativeBaseInfo CurrentBaseInfo;		// Current info is the current snapshot of the current base, with up-to-date transform that may be different than last-found.

		const bool bHasLastFoundInfo = SimBlackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, LastFoundBaseInfo);
		const bool bHasLastAppliedInfo = SimBlackboard->TryGet(CommonBlackboard::LastAppliedDynamicMovementBase, LastAppliedBaseInfo);
		if (bHasLastFoundInfo)
		{
			if (!bHasLastAppliedInfo || !LastFoundBaseInfo.UsesSameBase(LastAppliedBaseInfo))
			{
				LastAppliedBaseInfo = LastFoundBaseInfo;	// This is the first time we've checked this base, so start with the last-found capture
			}

			if (!ensureMsgf(LastFoundBaseInfo.HasRelativeInfo() && LastFoundBaseInfo.UsesSameBase(LastAppliedBaseInfo),
					TEXT("Attempting to update based movement with a missing or mismatched base. This may indicate a logic problem with detecting bases.")))
			{ 
				SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
				SimBlackboard->Invalidate(CommonBlackboard::LastAppliedDynamicMovementBase);
				return;
			}

			CurrentBaseInfo.SetFromComponent(LastFoundBaseInfo.MovementBase.Get(), LastFoundBaseInfo.BoneName);
			CurrentBaseInfo.ContactLocalPosition = LastFoundBaseInfo.ContactLocalPosition;

			FVector CurrentBaseLocation;
			FQuat CurrentBaseQuat;
			

			if (UBasedMovementUtils::GetMovementBaseTransform(CurrentBaseInfo.MovementBase.Get(), CurrentBaseInfo.BoneName, OUT CurrentBaseLocation, OUT CurrentBaseQuat))
			{
				const bool bDidBaseRotationChange = !LastAppliedBaseInfo.Rotation.Equals(CurrentBaseQuat, UE_SMALL_NUMBER);
				const bool bDidBaseLocationChange = (LastAppliedBaseInfo.Location != CurrentBaseLocation);

				FQuat DeltaQuat = FQuat::Identity;
				FVector WorldDeltaLocation = FVector::ZeroVector;
				FQuat WorldTargetQuat = UpdatedComponent->GetComponentQuat();

				// Find change in rotation

				if (bDidBaseRotationChange && !bIgnoreBaseRotation)
				{
					DeltaQuat = CurrentBaseQuat * LastAppliedBaseInfo.Rotation.Inverse();
					WorldTargetQuat = DeltaQuat * WorldTargetQuat;

					// TODO: make this respect the "up" direction, rather than assuming +Z = up
					FVector TargetForwVector = WorldTargetQuat.GetForwardVector();
					TargetForwVector.Z = 0.f;
					TargetForwVector.Normalize();

					WorldTargetQuat = UKismetMathLibrary::MakeRotFromX(TargetForwVector).Quaternion();
				}

				if (bDidBaseLocationChange || bDidBaseRotationChange)
				{
					// Calculate new transform matrix of base actor (ignoring scale).
					const FQuatRotationTranslationMatrix OldLocalToWorld(LastAppliedBaseInfo.Rotation, LastAppliedBaseInfo.Location);
					const FQuatRotationTranslationMatrix NewLocalToWorld(CurrentBaseQuat, CurrentBaseLocation);

					// Find change in location
					// NOTE that we are using the floor hit location, not the actor's root position which may be floating above the base
					const FVector NewWorldBaseContactPos = NewLocalToWorld.TransformPosition(CurrentBaseInfo.ContactLocalPosition);
					const FVector OldWorldBaseContactPos = OldLocalToWorld.TransformPosition(CurrentBaseInfo.ContactLocalPosition);
					WorldDeltaLocation = NewWorldBaseContactPos - OldWorldBaseContactPos;

					const FVector OldWorldLocation = UpdatedComponent->GetComponentLocation();
					EMoveComponentFlags MoveComponentFlags = MOVECOMP_IgnoreBases;
					const bool bSweep = true;
					FHitResult MoveHitResult;

					bool bDidMove = UMovementUtils::TryMoveUpdatedComponent_Internal(UpdatedComponent, WorldDeltaLocation, WorldTargetQuat, bSweep, MoveComponentFlags, &MoveHitResult, ETeleportType::None);
					
					const FVector NewWorldLocation = UpdatedComponent->GetComponentLocation();

					if ((NewWorldLocation - (OldWorldLocation + WorldDeltaLocation)).IsNearlyZero() == false)
					{
						// Find the remaining delta that wasn't achieved
						const FVector UnachievedWorldDelta = (OldWorldLocation + WorldDeltaLocation) - NewWorldLocation;

						// Convert the remaining delta to current base space
						FVector UnachievedLocalDelta;
						UBasedMovementUtils::TransformLocationToLocal(CurrentBaseLocation, CurrentBaseQuat, UnachievedWorldDelta, OUT UnachievedLocalDelta);
						
						// Subtract the remaining delta to reflect the change in the contact position
						CurrentBaseInfo.ContactLocalPosition -= UnachievedLocalDelta;
					}

					// Read, edit, write out the sync state based on the move results
					FMoverSyncState PendingSyncState;
					if (TargetMoverComp->BackendLiaisonComp->ReadPendingSyncState(OUT PendingSyncState))
					{
						if (FMoverDefaultSyncState* DefaultSyncState = PendingSyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>())
						{
							FVector Velocity = DefaultSyncState->GetVelocity_WorldSpace();
							DefaultSyncState->SetTransforms_WorldSpace(UpdatedComponent->GetComponentLocation(),
								UpdatedComponent->GetComponentRotation(),
								Velocity,
								CurrentBaseInfo.MovementBase.Get(), CurrentBaseInfo.BoneName);

							TargetMoverComp->BackendLiaisonComp->WritePendingSyncState(PendingSyncState);
						}
					}


				}

				SimBlackboard->Set(CommonBlackboard::LastAppliedDynamicMovementBase, CurrentBaseInfo);
				bDidGetUpToDate = true;
			}
		}
	}

	if (!bDidGetUpToDate)
	{
		SimBlackboard->Invalidate(CommonBlackboard::LastAppliedDynamicMovementBase);
	}
}



// FMoverDynamicBasedMovementTickFunction ////////////////////////////////////

void FMoverDynamicBasedMovementTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FActorComponentTickFunction::ExecuteTickHelper(TargetMoverComp, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
		{
			UBasedMovementUtils::UpdateSimpleBasedMovement(TargetMoverComp);
		});

	if (bAutoDisableAfterTick)
	{
		SetTickFunctionEnable(false);
	}
}
FString FMoverDynamicBasedMovementTickFunction::DiagnosticMessage()
{
	return TargetMoverComp->GetFullName() + TEXT("[FMoverDynamicBasedMovementTickFunction]");
}
FName FMoverDynamicBasedMovementTickFunction::DiagnosticContext(bool bDetailed)
{
	if (bDetailed)
	{
		return FName(*FString::Printf(TEXT("UMoverComponent/%s"), *GetFullNameSafe(TargetMoverComp)));
	}
	return FName(TEXT("FMoverDynamicBasedMovementTickFunction"));
}
