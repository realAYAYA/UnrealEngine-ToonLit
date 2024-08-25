// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlComponent.h"
#include "PhysicsControlLog.h"
#include "PhysicsControlComponentHelpers.h"

#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"

#include "Physics/Experimental/PhysScene_Chaos.h"

#include "Components/SkeletalMeshComponent.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

//UE_DISABLE_OPTIMIZATION;

//======================================================================================================================
// This file contains the non-public member functions of UPhysicsControlComponent
//======================================================================================================================

//======================================================================================================================
// Helper to get a valid skeletal mesh component pointer from a record
static USkeletalMeshComponent* GetValidSkeletalMeshComponentFromControlParent(
	const FPhysicsControlRecord& Record)
{
	return Cast<USkeletalMeshComponent>(Record.ParentMeshComponent.Get());
}

//======================================================================================================================
// Helper to get a valid skeletal mesh component pointer from a record
static USkeletalMeshComponent* GetValidSkeletalMeshComponentFromControlChild(
	const FPhysicsControlRecord& Record)
{
	return Cast<USkeletalMeshComponent>(Record.ChildMeshComponent.Get());
}

//======================================================================================================================
// Helper to get a valid skeletal mesh component pointer from a record
static USkeletalMeshComponent* GetValidSkeletalMeshComponentFromBodyModifier(
	const FPhysicsBodyModifierRecord& PhysicsBodyModifier)
{
	return Cast<USkeletalMeshComponent>(PhysicsBodyModifier.MeshComponent.Get());
}

//======================================================================================================================
bool UPhysicsControlComponent::GetBoneData(
	FCachedSkeletalMeshData::FBoneData& OutBoneData,
	const USkeletalMeshComponent*       InSkeletalMeshComponent,
	const FName                         InBoneName) const
{
	check(InSkeletalMeshComponent);
	const FReferenceSkeleton& RefSkeleton = InSkeletalMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(InBoneName);

	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogPhysicsControl, Warning, TEXT("Failed to find BoneIndex for %s"), *InBoneName.ToString());
		return false;
	}

	const FCachedSkeletalMeshData* CachedSkeletalMeshData = CachedSkeletalMeshDatas.Find(InSkeletalMeshComponent);
	if (CachedSkeletalMeshData &&
		CachedSkeletalMeshData->ReferenceCount > 0 &&
		!CachedSkeletalMeshData->BoneData.IsEmpty())
	{
		if (BoneIndex < CachedSkeletalMeshData->BoneData.Num())
		{
			OutBoneData = CachedSkeletalMeshData->BoneData[BoneIndex];
			return true;
		}
		UE_LOG(LogPhysicsControl, Warning, TEXT("BoneIndex is out of range"));

	}
	UE_LOG(LogPhysicsControl, Warning, TEXT("Failed to find bone data for %s"), *InBoneName.ToString());
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetModifiableBoneData(
	FCachedSkeletalMeshData::FBoneData*& OutBoneData,
	const USkeletalMeshComponent*        InSkeletalMeshComponent,
	const FName                          InBoneName)
{
	check(InSkeletalMeshComponent);
	const FReferenceSkeleton& RefSkeleton = InSkeletalMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(InBoneName);

	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogPhysicsControl, Warning, TEXT("Failed to find BoneIndex for %s"), *InBoneName.ToString());
		return false;
	}

	FCachedSkeletalMeshData* CachedSkeletalMeshData = CachedSkeletalMeshDatas.Find(InSkeletalMeshComponent);
	if (CachedSkeletalMeshData &&
		CachedSkeletalMeshData->ReferenceCount > 0 &&
		!CachedSkeletalMeshData->BoneData.IsEmpty())
	{
		if (BoneIndex < CachedSkeletalMeshData->BoneData.Num())
		{
			OutBoneData = &CachedSkeletalMeshData->BoneData[BoneIndex];
			return true;
		}
		UE_LOG(LogPhysicsControl, Warning, TEXT("BoneIndex is out of range"));

	}
	UE_LOG(LogPhysicsControl, Warning, TEXT("Failed to find bone data for %s"), *InBoneName.ToString());
	return false;
}


//======================================================================================================================
FPhysicsControlRecord* UPhysicsControlComponent::FindControlRecord(const FName Name)
{
	if (FPhysicsControlRecord* Record = ControlRecords.Find(Name))
	{
		return Record;
	}
	return nullptr;
}

//======================================================================================================================
const FPhysicsControlRecord* UPhysicsControlComponent::FindControlRecord(const FName Name) const
{
	if (const FPhysicsControlRecord* Record = ControlRecords.Find(Name))
	{
		return Record;
	}
	return nullptr;
}

//======================================================================================================================
FPhysicsControl* UPhysicsControlComponent::FindControl(const FName Name)
{
	if (FPhysicsControlRecord* Record = FindControlRecord(Name))
	{
		return &Record->PhysicsControl;
	}
	return nullptr;
}

//======================================================================================================================
const FPhysicsControl* UPhysicsControlComponent::FindControl(const FName Name) const
{
	if (const FPhysicsControlRecord* Record = FindControlRecord(Name))
	{
		return &Record->PhysicsControl;
	}
	return nullptr;
}

//======================================================================================================================
bool UPhysicsControlComponent::DetectTeleport(
	const FVector& OldPosition, const FQuat& OldOrientation,
	const FVector& NewPosition, const FQuat& NewOrientation) const
{
	if (TeleportDistanceThreshold > 0)
	{
		const double Distance = FVector::Distance(OldPosition, NewPosition);
		if (Distance > TeleportDistanceThreshold)
		{
			return true;
		}
	}
	if (TeleportRotationThreshold > 0)
	{
		const double Radians = OldOrientation.AngularDistance(NewOrientation);
		if (FMath::RadiansToDegrees(Radians) > TeleportRotationThreshold)
		{
			return true;
		}
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::DetectTeleport(const FTransform& OldTM, const FTransform& NewTM) const
{
	return DetectTeleport(OldTM.GetTranslation(), OldTM.GetRotation(), NewTM.GetTranslation(), NewTM.GetRotation());
}

//======================================================================================================================
void UPhysicsControlComponent::UpdateCachedSkeletalBoneData(float Dt)
{
	for (TPair<TWeakObjectPtr<USkeletalMeshComponent>, FCachedSkeletalMeshData>& CachedSkeletalMeshDataPair :
		CachedSkeletalMeshDatas)
	{
		FCachedSkeletalMeshData& CachedSkeletalMeshData = CachedSkeletalMeshDataPair.Value;
		if (!CachedSkeletalMeshData.ReferenceCount)
		{
			continue;
		}

		TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = CachedSkeletalMeshDataPair.Key;
		if (USkeletalMeshComponent* SkeletalMesh = SkeletalMeshComponent.Get())
		{
			const FTransform ComponentTM = SkeletalMesh->GetComponentTransform();
			const TArray<FTransform>& TMs = SkeletalMesh->GetEditableComponentSpaceTransforms();
			const int32 NumTMs = TMs.Num();
			if (NumTMs == CachedSkeletalMeshData.BoneData.Num() &&
				!DetectTeleport(CachedSkeletalMeshData.ComponentTM, ComponentTM))
			{
				// Avoid the Dt test on every bone
				if (Dt > 0)
				{
					for (int32 Index = 0; Index != NumTMs; ++Index)
					{
						FTransform TM = TMs[Index] * ComponentTM;
						CachedSkeletalMeshData.BoneData[Index].Update(TM.GetTranslation(), TM.GetRotation(), Dt);
					}
				}
				else
				{
					for (int32 Index = 0; Index != NumTMs; ++Index)
					{
						FTransform TM = TMs[Index] * ComponentTM;
						CachedSkeletalMeshData.BoneData[Index].Update(TM.GetTranslation(), TM.GetRotation());
					}
				}
			}
			else
			{
				CachedSkeletalMeshData.BoneData.Empty(NumTMs);
				for (const FTransform& BoneTM : TMs)
				{
					FTransform TM = BoneTM * ComponentTM;
					CachedSkeletalMeshData.BoneData.Emplace(TM.GetTranslation(), TM.GetRotation());
				}
			}
			CachedSkeletalMeshData.ComponentTM = ComponentTM;
		}
		else
		{
			CachedSkeletalMeshData.BoneData.Empty();
		}
	}
}

//======================================================================================================================
void UPhysicsControlComponent::ResetControls(bool bKeepControlRecords)
{
	for (TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair : ControlRecords)
	{
		FPhysicsControlRecord& Record = PhysicsControlRecordPair.Value;
		Record.ResetConstraint();
	}

	if (!bKeepControlRecords)
	{
		ControlRecords.Empty();
	}
}

//======================================================================================================================
void UPhysicsControlComponent::ApplyKinematicTarget(const FPhysicsBodyModifierRecord& Record) const
{
	// Seems like static and skeletal meshes need to be handled differently
	if (USkeletalMeshComponent* SkeletalMeshComponent = GetValidSkeletalMeshComponentFromBodyModifier(Record))
	{
		FBodyInstance* BodyInstance = UE::PhysicsControl::GetBodyInstance(
			Record.MeshComponent.Get(), Record.BodyModifier.BoneName);
		if (!BodyInstance)
		{
			return;
		}

		const FTransform TM = BodyInstance->GetUnrealWorldTransform(); // Preserve scale
		FTransform KinematicTarget = TM;
		KinematicTarget.SetRotation(Record.KinematicTargetOrientation);
		KinematicTarget.SetTranslation(Record.KinematicTargetPosition);
		if (Record.BodyModifier.ModifierData.bUseSkeletalAnimation)
		{
			FCachedSkeletalMeshData::FBoneData BoneData;
			if (GetBoneData(BoneData, SkeletalMeshComponent, Record.BodyModifier.BoneName))
			{
				FTransform BoneTM = BoneData.GetTM();
				KinematicTarget = KinematicTarget * BoneTM;
			}
		}
		ETeleportType TT = DetectTeleport(TM, KinematicTarget) ? ETeleportType::ResetPhysics : ETeleportType::None;
		BodyInstance->SetBodyTransform(KinematicTarget, TT);
	}
	else
	{
		const FTransform TM = Record.MeshComponent->GetComponentToWorld();
		const ETeleportType TT = DetectTeleport(
			TM.GetTranslation(), TM.GetRotation(), 
			Record.KinematicTargetPosition, Record.KinematicTargetOrientation)
			? ETeleportType::ResetPhysics : ETeleportType::None;
		// Note that calling BodyInstance->SetBodyTransform moves the physics, but not the mesh
		Record.MeshComponent->SetWorldLocationAndRotation(
			Record.KinematicTargetPosition, Record.KinematicTargetOrientation, false, nullptr, TT);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::ResetToCachedTarget(const FPhysicsBodyModifierRecord& Record) const
{
	FBodyInstance* BodyInstance = UE::PhysicsControl::GetBodyInstance(
		Record.MeshComponent.Get(), Record.BodyModifier.BoneName);
	if (!BodyInstance)
	{
		return;
	}

	if (USkeletalMeshComponent* SkeletalMeshComponent = GetValidSkeletalMeshComponentFromBodyModifier(Record))
	{
		FCachedSkeletalMeshData::FBoneData BoneData;
		if (GetBoneData(BoneData, SkeletalMeshComponent, Record.BodyModifier.BoneName))
		{
			FTransform BoneTM = BodyInstance->GetUnrealWorldTransform(); // Preserve scale
			BoneTM.SetLocation(BoneData.Position);
			BoneTM.SetRotation(BoneData.Orientation);

			BodyInstance->SetBodyTransform(BoneTM, ETeleportType::TeleportPhysics);
			BodyInstance->SetLinearVelocity(BoneData.Velocity, false);
			BodyInstance->SetAngularVelocityInRadians(BoneData.AngularVelocity, false);
		}
	}
}

//======================================================================================================================
void UPhysicsControlComponent::AddSkeletalMeshReferenceForCaching(
	USkeletalMeshComponent* InSkeletalMeshComponent)
{
	check(InSkeletalMeshComponent);
	for (TPair<TWeakObjectPtr<USkeletalMeshComponent>, FCachedSkeletalMeshData>& CachedSkeletalMeshDataPair :
		CachedSkeletalMeshDatas)
	{
		TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = CachedSkeletalMeshDataPair.Key;
		if (SkeletalMeshComponent.Get() == InSkeletalMeshComponent)
		{
			FCachedSkeletalMeshData& CachedSkeletalMeshData = CachedSkeletalMeshDataPair.Value;
			++CachedSkeletalMeshData.ReferenceCount;
			return;
		}
	}
	FCachedSkeletalMeshData& Data = CachedSkeletalMeshDatas.Add(InSkeletalMeshComponent);
	Data.ReferenceCount = 1;
	PrimaryComponentTick.AddPrerequisite(InSkeletalMeshComponent, InSkeletalMeshComponent->PrimaryComponentTick);
}

//======================================================================================================================
bool UPhysicsControlComponent::RemoveSkeletalMeshReferenceForCaching(
	USkeletalMeshComponent* InSkeletalMeshComponent)
{
	check(InSkeletalMeshComponent);
	if (!InSkeletalMeshComponent)
	{
		UE_LOG(LogPhysicsControl, Warning, TEXT("Invalid skeletal mesh component"));
		return false;
	}

	for (auto It = CachedSkeletalMeshDatas.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = It.Key();
		FCachedSkeletalMeshData& Data = It.Value();
		if (SkeletalMeshComponent.Get() == InSkeletalMeshComponent)
		{
			if (--Data.ReferenceCount == 0)
			{
				PrimaryComponentTick.RemovePrerequisite(
					InSkeletalMeshComponent, InSkeletalMeshComponent->PrimaryComponentTick);
				It.RemoveCurrent();
				return true;
			}
			return false;
		}
	}
	UE_LOG(LogPhysicsControl, Warning, TEXT("Failed to remove skeletal mesh component reference for caching"));
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::AddSkeletalMeshReferenceForModifier(
	USkeletalMeshComponent* InSkeletalMeshComponent)
{
	check(InSkeletalMeshComponent);
	for (TPair<TWeakObjectPtr<USkeletalMeshComponent>, FModifiedSkeletalMeshData>& ModifiedSkeletalMeshDataPair :
		ModifiedSkeletalMeshDatas)
	{
		TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = ModifiedSkeletalMeshDataPair.Key;
		if (SkeletalMeshComponent.Get() == InSkeletalMeshComponent)
		{
			FModifiedSkeletalMeshData& ModifiedSkeletalMeshData = ModifiedSkeletalMeshDataPair.Value;
			++ModifiedSkeletalMeshData.ReferenceCount;
			return;
		}
	}
	FModifiedSkeletalMeshData& Data = ModifiedSkeletalMeshDatas.Add(InSkeletalMeshComponent);
	Data.ReferenceCount = 1;
	Data.bOriginalUpdateMeshWhenKinematic = InSkeletalMeshComponent->bUpdateMeshWhenKinematic;
	Data.OriginalKinematicBonesUpdateType = InSkeletalMeshComponent->KinematicBonesUpdateType;
	InSkeletalMeshComponent->bUpdateMeshWhenKinematic = true;
	// By default, kinematic bodies will have their blend weight set to zero. This is a problem for us since:
	// 1. We expect there will be lots of cases where only part of the character is dynamic, and other 
	//    parts are kinematic
	// 2. If those parts are towards the root of the character, then if their physics blend weight is zero, 
	//    they are unable to "move away" from the component - e.g. if the component itself is moved by the 
	//    movement component
	// 3. We want to support users using the physics blend weight, so we can't simply force a physics blend 
	//    weight of 1 in the skeletal mesh component (PhysAnim.cpp).
	// So, we set all the bodies to have a blend weight of 1, noting that any under the control of a BodyModifier
	// will get updated each tick.
	InSkeletalMeshComponent->SetAllBodiesPhysicsBlendWeight(1.0f);
}

//======================================================================================================================
bool UPhysicsControlComponent::RemoveSkeletalMeshReferenceForModifier(
	USkeletalMeshComponent* InSkeletalMeshComponent)
{
	check(InSkeletalMeshComponent);
	if (!InSkeletalMeshComponent)
	{
		UE_LOG(LogPhysicsControl, Warning, TEXT("Invalid skeletal mesh component"));
		return false;
	}

	for (auto It = ModifiedSkeletalMeshDatas.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = It.Key();
		FModifiedSkeletalMeshData& Data = It.Value();
		if (SkeletalMeshComponent.Get() == InSkeletalMeshComponent)
		{
			if (--Data.ReferenceCount == 0)
			{
				InSkeletalMeshComponent->bUpdateMeshWhenKinematic = Data.bOriginalUpdateMeshWhenKinematic;
				InSkeletalMeshComponent->KinematicBonesUpdateType = Data.OriginalKinematicBonesUpdateType;
				It.RemoveCurrent();
				return true;
			}
			return false;
		}
	}
	UE_LOG(LogPhysicsControl, Warning, TEXT("Failed to remove skeletal mesh component reference for modifier"));
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::CalculateControlTargetData(
	FTransform&                  OutTargetTM,
	FVector&                     OutTargetVelocity,
	FVector&                     OutTargetAngularVelocity,
	const FPhysicsControlRecord& Record,
	bool                         bCalculateVelocity) const
{
	OutTargetTM = FTransform();
	OutTargetVelocity.Set(0, 0, 0);
	OutTargetAngularVelocity.Set(0, 0, 0);

	bool bUsedSkeletalAnimation = false;

	// Set the target TM and velocities based on any skeletal action. Note that the targets from animation 
	// should always account for the control point
	if (Record.PhysicsControl.ControlData.bUseSkeletalAnimation)
	{
		FCachedSkeletalMeshData::FBoneData ChildBoneData, ParentBoneData;
		bool bHaveChildBoneData = false;
		bool bHaveParentBoneData = false;

		if (USkeletalMeshComponent* ChildSkeletalMeshComponent = GetValidSkeletalMeshComponentFromControlChild(Record))
		{
			bHaveChildBoneData = GetBoneData(
				ChildBoneData, ChildSkeletalMeshComponent, Record.PhysicsControl.ChildBoneName);
		}

		if (USkeletalMeshComponent* ParentSkeletalMeshComponent = GetValidSkeletalMeshComponentFromControlParent(Record))
		{
			bHaveParentBoneData = GetBoneData(
				ParentBoneData, ParentSkeletalMeshComponent, Record.PhysicsControl.ParentBoneName);
		}
		else if (Record.ParentMeshComponent.IsValid())
		{
			const FTransform ParentTM = Record.ParentMeshComponent->GetComponentTransform();
			ParentBoneData.Position = ParentTM.GetLocation();
			ParentBoneData.Orientation = ParentTM.GetRotation();
			ParentBoneData.Velocity = Record.ParentMeshComponent->GetPhysicsLinearVelocity();
			ParentBoneData.AngularVelocity = Record.ParentMeshComponent->GetPhysicsAngularVelocityInRadians();
			bHaveParentBoneData = true;
			bCalculateVelocity = false;
		}

		// Note that the TargetTM/velocity calculated so far are supposed to be interpreted as
		// expressed relative to the skeletal animation pose.
		//
		// Also note that the velocities calculated in the bone data are the strict rates of change
		// of the transform position/orientation - not of the center of mass (which is what physics
		// bodies often use for velocity).
		if (bHaveChildBoneData)
		{
			bUsedSkeletalAnimation = true;
			const FTransform ChildBoneTM = ChildBoneData.GetTM();
			if (bHaveParentBoneData)
			{
				const FTransform ParentBoneTM = ParentBoneData.GetTM();
				const FTransform SkeletalDeltaTM = ChildBoneTM * ParentBoneTM.Inverse();
				// This puts TargetTM in the space of the ParentBone
				OutTargetTM = SkeletalDeltaTM;

				// Add on the control point offset
				OutTargetTM.AddToTranslation(OutTargetTM.GetRotation() * Record.GetControlPoint());

				const FQuat ParentBoneQ = ParentBoneTM.GetRotation();
				const FQuat ParentBoneInvQ = ParentBoneQ.Inverse();

				if (bCalculateVelocity)
				{
					if (Record.PhysicsControl.ControlData.SkeletalAnimationVelocityMultiplier != 0)
					{
						// Offset of the control point from the target child bone TM, in world space.
						const FVector WorldControlPointOffset = ChildBoneTM.GetRotation() * Record.GetControlPoint();
						// World space position of the target control point
						const FVector WorldChildControlPointPosition = ChildBoneTM.GetTranslation() + WorldControlPointOffset;

						// World-space velocity of the control point due to the motion of the parent
						// linear and angular velocity.
						const FVector ChildTargetVelocityDueToParent =
							ParentBoneData.Velocity + ParentBoneData.AngularVelocity.Cross(
								WorldChildControlPointPosition - ParentBoneTM.GetTranslation());
						// World-space velocity of the control point due to the motion of the child
						// linear and angular velocity
						const FVector ChildTargetVelocity =
							ChildBoneData.Velocity + ChildBoneData.AngularVelocity.Cross(WorldControlPointOffset);

						// Pull out just the motion in the child that isn't due to the parent
						const FVector SkeletalTargetVelocity =
							ParentBoneInvQ * (ChildTargetVelocity - ChildTargetVelocityDueToParent);
						OutTargetVelocity += SkeletalTargetVelocity *
							Record.PhysicsControl.ControlData.SkeletalAnimationVelocityMultiplier;

						const FVector SkeletalTargetAngularVelocity =
							ParentBoneInvQ * (ChildBoneData.AngularVelocity - ParentBoneData.AngularVelocity);
						OutTargetAngularVelocity += SkeletalTargetAngularVelocity *
							Record.PhysicsControl.ControlData.SkeletalAnimationVelocityMultiplier;
					}
				}
			}
			else
			{
				OutTargetTM = ChildBoneTM;

				// Add on the control point offset
				OutTargetTM.AddToTranslation(OutTargetTM.GetRotation()* Record.GetControlPoint());

				if (bCalculateVelocity)
				{
					OutTargetVelocity = ChildBoneTM.GetRotation() * OutTargetVelocity;
					OutTargetAngularVelocity = ChildBoneTM.GetRotation() * OutTargetAngularVelocity;

					if (Record.PhysicsControl.ControlData.SkeletalAnimationVelocityMultiplier != 0)
					{
						const FVector WorldControlPointOffset = ChildBoneTM.GetRotation() * Record.GetControlPoint();
						const FVector WorldChildControlPointPosition = ChildBoneTM.GetTranslation() + WorldControlPointOffset;

						// const World-space velocity of the control point due to the motion of the child
						const FVector ChildTargetVelocity =
							ChildBoneData.Velocity + ChildBoneData.AngularVelocity.Cross(WorldControlPointOffset);

						OutTargetVelocity += ChildTargetVelocity *
							Record.PhysicsControl.ControlData.SkeletalAnimationVelocityMultiplier;

						OutTargetAngularVelocity += ChildBoneData.AngularVelocity *
							Record.PhysicsControl.ControlData.SkeletalAnimationVelocityMultiplier;
					}
				}
			}
		}
	}

	// Now apply the explicit target specified in the record. It operates in the space of the target
	// transform we (may have) just calculated.
	{
		const FPhysicsControlTarget& Target = Record.ControlTarget;

		// Calculate the authored target position/orientation - i.e. not using the skeletal animation
		FQuat TargetOrientationQ = Target.TargetOrientation.Quaternion();
		const FVector TargetPosition = Target.TargetPosition;

		FVector ExtraTargetPosition(0);
		// Incorporate the offset from the control point. If we used animation, then we don't need
		// to do this.
		if (!bUsedSkeletalAnimation && 
			Record.ControlTarget.bApplyControlPointToTarget)
		{
			ExtraTargetPosition = TargetOrientationQ * Record.GetControlPoint();
		}

		// The record's target is specified in the space of the previously calculated/set OutTargetTM
		OutTargetTM = FTransform(TargetOrientationQ, TargetPosition + ExtraTargetPosition) * OutTargetTM;

		if (bCalculateVelocity)
		{
			// Note that Target.TargetAngularVelocity is in revs per second (as it's user-facing)
			const FVector TargetAngularVelocity = Target.TargetAngularVelocity * UE_TWO_PI;
			OutTargetAngularVelocity += TargetAngularVelocity;
			const FVector ExtraVelocity = TargetAngularVelocity.Cross(ExtraTargetPosition);
			OutTargetVelocity += Target.TargetVelocity + ExtraVelocity;
		}
	}
}

//======================================================================================================================
bool UPhysicsControlComponent::ApplyControlStrengths(
	FPhysicsControlRecord& Record, FConstraintInstance* ConstraintInstance)
{
	const FPhysicsControlData& Data = Record.PhysicsControl.ControlData;
	const FPhysicsControlMultiplier& Multiplier = Record.PhysicsControl.ControlMultiplier;

	float AngularSpring;
	float AngularDamping;
	const float MaxTorque = Data.MaxTorque * Multiplier.MaxTorqueMultiplier;

	FVector LinearSpring;
	FVector LinearDamping;
	const FVector MaxForce = Data.MaxForce * Multiplier.MaxForceMultiplier;

	UE::PhysicsControl::ConvertStrengthToSpringParams(
		AngularSpring, AngularDamping,
		Data.AngularStrength * Multiplier.AngularStrengthMultiplier,
		Data.AngularDampingRatio * Multiplier.AngularDampingRatioMultiplier,
		Data.AngularExtraDamping * Multiplier.AngularExtraDampingMultiplier);
	UE::PhysicsControl::ConvertStrengthToSpringParams(
		LinearSpring, LinearDamping,
		Data.LinearStrength * Multiplier.LinearStrengthMultiplier,
		Data.LinearDampingRatio * Multiplier.LinearDampingRatioMultiplier,
		Data.LinearExtraDamping * Multiplier.LinearExtraDampingMultiplier);

	if (Multiplier.MaxTorqueMultiplier <= 0)
	{
		AngularSpring = 0;
		AngularDamping = 0;
	}
	if (Multiplier.MaxForceMultiplier.X <= 0)
	{
		LinearSpring.X = 0;
		LinearDamping.X = 0;
	}
	if (Multiplier.MaxForceMultiplier.Y <= 0)
	{
		LinearSpring.Y = 0;
		LinearDamping.Y = 0;
	}
	if (Multiplier.MaxForceMultiplier.Z <= 0)
	{
		LinearSpring.Z = 0;
		LinearDamping.Z = 0;
	}

	ConstraintInstance->SetDriveParams(
		LinearSpring, LinearDamping, MaxForce,
		FVector(0, 0, AngularSpring), FVector(0, 0, AngularDamping), FVector(0, 0, MaxTorque),
		EAngularDriveMode::SLERP);

	const bool bHaveAngular = (AngularSpring + AngularDamping) > 0;
	const bool bHaveLinear = (LinearSpring + LinearDamping).GetMax() > 0;
	return bHaveLinear || bHaveAngular;
}

//======================================================================================================================
void UPhysicsControlComponent::ApplyControl(FPhysicsControlRecord& Record)
{
	FConstraintInstance* ConstraintInstance = Record.ConstraintInstance.Get();

	if (!ConstraintInstance)
	{
		return;
	}

	if (!Record.PhysicsControl.IsEnabled())
	{
		// Note that this will disable the constraint elements when strength/damping are zero
		ConstraintInstance->SetDriveParams(
			FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector,
			FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector,
			EAngularDriveMode::SLERP);
		return;
	}

	// Always control collision, because otherwise maintaining it is very difficult, since
	// constraint-controlled collision doesn't interact nicely when there are multiple constraints.
	ConstraintInstance->SetDisableCollision(Record.PhysicsControl.ControlData.bDisableCollision);

	FBodyInstance* ParentBodyInstance = UE::PhysicsControl::GetBodyInstance(
		Record.ParentMeshComponent.Get(), Record.PhysicsControl.ParentBoneName);

	FBodyInstance* ChildBodyInstance = UE::PhysicsControl::GetBodyInstance(
		Record.ChildMeshComponent.Get(), Record.PhysicsControl.ChildBoneName);

	if (!ParentBodyInstance && !ChildBodyInstance)
	{
		return;
	}

	// Set strengths etc and then targets (if there were strengths)
	if (ApplyControlStrengths(Record, ConstraintInstance))
	{
		FTransform TargetTM;
		FVector TargetVelocity;
		FVector TargetAngularVelocity;
		CalculateControlTargetData(TargetTM, TargetVelocity, TargetAngularVelocity, Record, true);

		ConstraintInstance->SetLinearPositionTarget(TargetTM.GetTranslation());
		ConstraintInstance->SetAngularOrientationTarget(TargetTM.GetRotation());
		ConstraintInstance->SetLinearVelocityTarget(TargetVelocity);
		ConstraintInstance->SetAngularVelocityTarget(TargetAngularVelocity / UE_TWO_PI); // In rev/sec
		ConstraintInstance->SetParentDominates(Record.PhysicsControl.ControlData.bOnlyControlChildObject);

		if (ParentBodyInstance)
		{
			ParentBodyInstance->WakeInstance();
		}
		if (ChildBodyInstance)
		{
			ChildBodyInstance->WakeInstance();
		}
	}
}

//======================================================================================================================
void UPhysicsControlComponent::ApplyBodyModifier(FPhysicsBodyModifierRecord& Record)
{
	USkeletalMeshComponent* SKM = GetValidSkeletalMeshComponentFromBodyModifier(Record);
	FBodyInstance* BodyInstance = UE::PhysicsControl::GetBodyInstance(
		Record.MeshComponent.Get(), Record.BodyModifier.BoneName);
	if (BodyInstance)
	{
		switch (Record.BodyModifier.ModifierData.MovementType)
		{
		case EPhysicsMovementType::Static:
			BodyInstance->SetInstanceSimulatePhysics(false, false, true);
			break;
		case EPhysicsMovementType::Kinematic:
			BodyInstance->SetInstanceSimulatePhysics(false, false, true);
			ApplyKinematicTarget(Record);
			break;
		case EPhysicsMovementType::Simulated:
			BodyInstance->SetInstanceSimulatePhysics(true, false, true);
			break;
		case EPhysicsMovementType::Default:
			// Default means do nothing, so let's do exactly that
			break;
		default:
			UE_LOG(LogPhysicsControl, Warning, TEXT("Invalid movement type %d"),
				int(Record.BodyModifier.ModifierData.MovementType));
			break;
		}

		// We always overwrite the physics blend weight, since the functions above can still modify
		// it (even though they all use the "maintain physics blending" option), since there is an
		// expectation that zero blend weight means to disable physics.
		BodyInstance->PhysicsBlendWeight = Record.BodyModifier.ModifierData.PhysicsBlendWeight;
		BodyInstance->SetUpdateKinematicFromSimulation(Record.BodyModifier.ModifierData.bUpdateKinematicFromSimulation);

		UBodySetup* BodySetup = BodyInstance->GetBodySetup();
		if (BodySetup)
		{
			int32 NumShapes = BodySetup->AggGeom.GetElementCount();
			for (int32 ShapeIndex = 0; ShapeIndex != NumShapes; ++ShapeIndex)
			{
				BodyInstance->SetShapeCollisionEnabled(ShapeIndex, Record.BodyModifier.ModifierData.CollisionType);
			}
		}

		if (BodyInstance->IsInstanceSimulatingPhysics())
		{
			const float GravityZ = BodyInstance->GetPhysicsScene()->GetOwningWorld()->GetGravityZ();
			const float AppliedGravityZ = BodyInstance->bEnableGravity ? GravityZ : 0.0f;
			const float DesiredGravityZ = GravityZ * Record.BodyModifier.ModifierData.GravityMultiplier;
			const float GravityZToApply = DesiredGravityZ - AppliedGravityZ;
			BodyInstance->AddForce(FVector(0, 0, GravityZToApply), true, true);
		}
	}
	if (Record.bResetToCachedTarget)
	{
		Record.bResetToCachedTarget = false;
		ResetToCachedTarget(Record);
	}
}

//======================================================================================================================
FPhysicsBodyModifierRecord* UPhysicsControlComponent::FindBodyModifierRecord(const FName Name)
{
	return BodyModifierRecords.Find(Name);
}

//======================================================================================================================
const FPhysicsBodyModifierRecord* UPhysicsControlComponent::FindBodyModifierRecord(const FName Name) const
{
	return BodyModifierRecords.Find(Name);
}

//======================================================================================================================
bool UPhysicsControlComponent::DestroyControl(const FName Name, const EDestroyBehavior DestroyBehavior)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = GetValidSkeletalMeshComponentFromControlParent(*Record))
		{
			RemoveSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		}
		if (USkeletalMeshComponent* SkeletalMeshComponent = GetValidSkeletalMeshComponentFromControlChild(*Record))
		{
			RemoveSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		}

		Record->ResetConstraint();
		NameRecords.RemoveControl(Name);
		if (DestroyBehavior == EDestroyBehavior::RemoveRecord)
		{
			check(ControlRecords.Remove(Name) == 1);
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("DestroyControl - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::DestroyBodyModifier(const FName Name, const EDestroyBehavior DestroyBehavior)
{
	FPhysicsBodyModifierRecord* BodyModifier = FindBodyModifierRecord(Name);
	if (BodyModifier)
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = GetValidSkeletalMeshComponentFromBodyModifier(*BodyModifier))
		{
			RemoveSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
			RemoveSkeletalMeshReferenceForModifier(SkeletalMeshComponent);
		}
		NameRecords.RemoveBodyModifier(Name);
		if (DestroyBehavior == EDestroyBehavior::RemoveRecord)
		{
			check(BodyModifierRecords.Remove(Name) == 1);
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("DestroyBodyModifier - invalid name %s"), *Name.ToString());
	}
	return false;
}

