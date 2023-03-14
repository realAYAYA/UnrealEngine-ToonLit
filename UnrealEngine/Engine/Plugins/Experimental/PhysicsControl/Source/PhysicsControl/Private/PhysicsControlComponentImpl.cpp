// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlComponentImpl.h"
#include "PhysicsControlComponentLog.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlComponentHelpers.h"

#include "Components/SkeletalMeshComponent.h"

//======================================================================================================================
bool FPhysicsControlComponentImpl::GetBoneData(
	FCachedSkeletalMeshData::FBoneData& OutBoneData,
	const USkeletalMeshComponent*       InSkeletalMeshComponent,
	const FName                         InBoneName) const
{
	check(InSkeletalMeshComponent);
	const FReferenceSkeleton& RefSkeleton = InSkeletalMeshComponent->GetSkeletalMeshAsset()->GetRefSkeleton();
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(InBoneName);

	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to find BoneIndex for %s"), *InBoneName.ToString());
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
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("BoneIndex is out of range"));

	}
	UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to find bone data for %s"), *InBoneName.ToString());
	return false;
}

//======================================================================================================================
FPhysicsControlRecord* FPhysicsControlComponentImpl::FindControlRecord(const FName Name)
{
	if (FPhysicsControlRecord* Record = PhysicsControlRecords.Find(Name))
	{
		return Record;
	}
	return nullptr;
}

//======================================================================================================================
FPhysicsControl* FPhysicsControlComponentImpl::FindControl(const FName Name)
{
	if (FPhysicsControlRecord* ControlRecord = FindControlRecord(Name))
	{
		return &ControlRecord->PhysicsControl;
	}
	return nullptr;
}

//======================================================================================================================
bool FPhysicsControlComponentImpl::DetectTeleport(
	const FVector& OldPosition, const FQuat& OldOrientation,
	const FVector& NewPosition, const FQuat& NewOrientation) const
{
	if (Owner->TeleportDistanceThreshold > 0)
	{
		double Distance = FVector::Distance(OldPosition, NewPosition);
		if (Distance > Owner->TeleportDistanceThreshold)
		{
			return true;
		}
	}
	if (Owner->TeleportRotationThreshold > 0)
	{
		double Radians = OldOrientation.AngularDistance(NewOrientation);
		if (FMath::RadiansToDegrees(Radians) > Owner->TeleportRotationThreshold)
		{
			return true;
		}
	}
	return false;
}

//======================================================================================================================
bool FPhysicsControlComponentImpl::DetectTeleport(const FTransform& OldTM, const FTransform& NewTM) const
{
	return DetectTeleport(OldTM.GetTranslation(), OldTM.GetRotation(), NewTM.GetTranslation(), NewTM.GetRotation());
}

//======================================================================================================================
void FPhysicsControlComponentImpl::UpdateCachedSkeletalBoneData(float Dt)
{
	for (TPair<TObjectPtr<USkeletalMeshComponent>, FCachedSkeletalMeshData>& CachedSkeletalMeshDataPair :
		CachedSkeletalMeshDatas)
	{
		FCachedSkeletalMeshData& CachedSkeletalMeshData = CachedSkeletalMeshDataPair.Value;
		if (!CachedSkeletalMeshData.ReferenceCount)
		{
			continue;
		}

		TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = CachedSkeletalMeshDataPair.Key;
		if (USkeletalMeshComponent* SkeletalMesh = SkeletalMeshComponent.Get())
		{
			FTransform ComponentTM = SkeletalMesh->GetComponentToWorld();
			const TArray<FTransform>& TMs = SkeletalMesh->GetEditableComponentSpaceTransforms();
			if (TMs.Num() == CachedSkeletalMeshData.BoneData.Num() &&
				!DetectTeleport(CachedSkeletalMeshData.ComponentTM, ComponentTM))
			{
				for (int32 Index = 0; Index != TMs.Num(); ++Index)
				{
					FTransform TM = TMs[Index] * ComponentTM;
					CachedSkeletalMeshData.BoneData[Index].Update(TM.GetTranslation(), TM.GetRotation(), Dt);
				}
			}
			else
			{
				CachedSkeletalMeshData.BoneData.Empty(TMs.Num());
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
void FPhysicsControlComponentImpl::ResetControls(bool bKeepControlRecords)
{
	for (TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair : PhysicsControlRecords)
	{
		FPhysicsControlRecord& Record = PhysicsControlRecordPair.Value;
		Record.PhysicsControlState.Reset();
	}

	if (!bKeepControlRecords)
	{
		PhysicsControlRecords.Empty();
	}
}

//======================================================================================================================
void FPhysicsControlComponentImpl::ApplyKinematicTarget(const FPhysicsBodyModifier& BodyModifier) const
{
	// Seems like static and skeletal meshes need to be handled differently
	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(BodyModifier.MeshComponent.Get()))
	{
		FBodyInstance* BodyInstance = UE::PhysicsControlComponent::GetBodyInstance(
			BodyModifier.MeshComponent, BodyModifier.BoneName);
		if (!BodyInstance)
		{
			return;
		}

		FTransform TM = BodyInstance->GetUnrealWorldTransform();
		FTransform KinematicTarget = TM;
		KinematicTarget.SetRotation(BodyModifier.KinematicTargetOrientation);
		KinematicTarget.SetTranslation(BodyModifier.KinematicTargetPosition);
		if (BodyModifier.bUseSkeletalAnimation)
		{
			FCachedSkeletalMeshData::FBoneData BoneData;
			if (GetBoneData(BoneData, SkeletalMeshComponent, BodyModifier.BoneName))
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
		FTransform TM = BodyModifier.MeshComponent->GetComponentToWorld();
		ETeleportType TT = DetectTeleport(
			TM.GetTranslation(), TM.GetRotation(), 
			BodyModifier.KinematicTargetPosition, BodyModifier.KinematicTargetOrientation) 
			? ETeleportType::ResetPhysics : ETeleportType::None;
		// Note that calling BodyInstance->SetBodyTransform moves the physics, but not the mesh
		BodyModifier.MeshComponent->SetWorldLocationAndRotation(
			BodyModifier.KinematicTargetPosition, BodyModifier.KinematicTargetOrientation, false, nullptr, TT);
	}
}

//======================================================================================================================
void FPhysicsControlComponentImpl::AddSkeletalMeshReferenceForCaching(
	USkeletalMeshComponent* InSkeletalMeshComponent)
{
	check(InSkeletalMeshComponent);
	for (TPair<TObjectPtr<USkeletalMeshComponent>, FCachedSkeletalMeshData>& CachedSkeletalMeshDataPair :
		CachedSkeletalMeshDatas)
	{
		TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = CachedSkeletalMeshDataPair.Key;
		if (SkeletalMeshComponent == InSkeletalMeshComponent)
		{
			FCachedSkeletalMeshData& CachedSkeletalMeshData = CachedSkeletalMeshDataPair.Value;
			++CachedSkeletalMeshData.ReferenceCount;
			return;
		}
	}
	FCachedSkeletalMeshData& Data = CachedSkeletalMeshDatas.Add(InSkeletalMeshComponent);
	Data.ReferenceCount = 1;
	Owner->PrimaryComponentTick.AddPrerequisite(InSkeletalMeshComponent, InSkeletalMeshComponent->PrimaryComponentTick);
}

//======================================================================================================================
bool FPhysicsControlComponentImpl::RemoveSkeletalMeshReferenceForCaching(
	USkeletalMeshComponent* InSkeletalMeshComponent)
{
	check(InSkeletalMeshComponent);
	if (!InSkeletalMeshComponent)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Invalid skeletal mesh component"));
		return false;
	}

	for (auto It = CachedSkeletalMeshDatas.CreateIterator(); It; ++It)
	{
		TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = It.Key();
		FCachedSkeletalMeshData& Data = It.Value();
		if (SkeletalMeshComponent == InSkeletalMeshComponent)
		{
			if (--Data.ReferenceCount == 0)
			{
				Owner->PrimaryComponentTick.RemovePrerequisite(
					InSkeletalMeshComponent, InSkeletalMeshComponent->PrimaryComponentTick);
				It.RemoveCurrent();
				return true;
			}
			return false;
		}
	}
	UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to remove skeletal mesh component reference for caching"));
	return false;
}

//======================================================================================================================
void FPhysicsControlComponentImpl::AddSkeletalMeshReferenceForModifier(
	USkeletalMeshComponent* InSkeletalMeshComponent)
{
	check(InSkeletalMeshComponent);
	for (TPair<TObjectPtr<USkeletalMeshComponent>, FModifiedSkeletalMeshData>& ModifiedSkeletalMeshDataPair :
		ModifiedSkeletalMeshDatas)
	{
		TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = ModifiedSkeletalMeshDataPair.Key;
		if (SkeletalMeshComponent == InSkeletalMeshComponent)
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
	InSkeletalMeshComponent->KinematicBonesUpdateType = EKinematicBonesUpdateToPhysics::SkipAllBones;
}

//======================================================================================================================
bool FPhysicsControlComponentImpl::RemoveSkeletalMeshReferenceForModifier(
	USkeletalMeshComponent* InSkeletalMeshComponent)
{
	check(InSkeletalMeshComponent);
	if (!InSkeletalMeshComponent)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Invalid skeletal mesh component"));
		return false;
	}

	for (auto It = ModifiedSkeletalMeshDatas.CreateIterator(); It; ++It)
	{
		TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = It.Key();
		FModifiedSkeletalMeshData& Data = It.Value();
		if (SkeletalMeshComponent == InSkeletalMeshComponent)
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
	UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to remove skeletal mesh component reference for modifier"));
	return false;
}

//======================================================================================================================
FName FPhysicsControlComponentImpl::GetUniqueControlName(const FName ParentBoneName, const FName ChildBoneName) const
{
	FString NameBase = TEXT("");
	if (!ParentBoneName.IsNone())
	{
		NameBase += ParentBoneName.ToString() + TEXT("_");
	}
	if (!ChildBoneName.IsNone())
	{
		NameBase += ChildBoneName.ToString() + TEXT("_");
	}

	TSet<FName> Keys;
	PhysicsControlRecords.GetKeys(Keys);
	// If the number gets too large, almost certainly we're in some nasty situation where this is
	// getting called in a loop. Better to quit and fail, rather than allow the constraint set to
	// increase without bound. 
	for (int32 Index = 0; Index < Owner->MaxNumControlsOrModifiersPerName; ++Index)
	{
		FString NameStr = FString::Format(TEXT("{0}{1}"), { NameBase, Index });
		FName Name(NameStr);
		if (!Keys.Find(Name))
		{
			return Name;
		}
	}
	UE_LOG(LogPhysicsControlComponent, Warning,
		TEXT("Unable to find a suitable Control name - the limit of MaxNumControlsOrModifiersPerName (%d) has been exceeded"),
		Owner->MaxNumControlsOrModifiersPerName);
	return FName();
}

//======================================================================================================================
FName FPhysicsControlComponentImpl::GetUniqueBodyModifierName(const FName BoneName) const
{
	FString NameBase = TEXT("");
	if (!BoneName.IsNone())
	{
		NameBase += BoneName.ToString() + TEXT("_");
	}
	else
	{
		NameBase = TEXT("Body_");
	}

	TSet<FName> Keys;
	PhysicsBodyModifiers.GetKeys(Keys);
	// If the number gets too large, almost certainly we're in some nasty situation where this is
	// getting called in a loop. Better to quit and fail, rather than allow the modifier set to
	// increase without bound. 
	for (int32 Index = 0; Index != Owner->MaxNumControlsOrModifiersPerName; ++Index)
	{
		FString NameStr = FString::Format(TEXT("{0}{1}"), { NameBase, Index });
		FName Name(NameStr);
		if (!Keys.Find(Name))
		{
			return Name;
		}
	}
	UE_LOG(LogPhysicsControlComponent, Warning,
		TEXT("Unable to find a suitable Body Modifier name - the limit of MaxNumControlsOrModifiersPerName (%d) has been exceeded"),
		Owner->MaxNumControlsOrModifiersPerName);
	return FName();
}

//======================================================================================================================
void FPhysicsControlComponentImpl::CalculateControlTargetData(
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
	if (Record.PhysicsControl.ControlSettings.bUseSkeletalAnimation)
	{
		FCachedSkeletalMeshData::FBoneData ChildBoneData, ParentBoneData;
		bool bHaveChildBoneData = false;
		bool bHaveParentBoneData = false;

		if (USkeletalMeshComponent* ChildSkeletalMeshComponent =
			Cast<USkeletalMeshComponent>(Record.PhysicsControl.ChildMeshComponent.Get()))
		{
			bHaveChildBoneData = GetBoneData(
				ChildBoneData, ChildSkeletalMeshComponent, Record.PhysicsControl.ChildBoneName);
		}

		if (USkeletalMeshComponent* ParentSkeletalMeshComponent =
			Cast<USkeletalMeshComponent>(Record.PhysicsControl.ParentMeshComponent.Get()))
		{
			bHaveParentBoneData = GetBoneData(
				ParentBoneData, ParentSkeletalMeshComponent, Record.PhysicsControl.ParentBoneName);
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
			FTransform ChildBoneTM = ChildBoneData.GetTM();
			if (bHaveParentBoneData)
			{
				FTransform ParentBoneTM = ParentBoneData.GetTM();
				FTransform SkeletalDeltaTM = ChildBoneTM * ParentBoneTM.Inverse();
				// This puts TargetTM in the space of the ParentBone
				OutTargetTM = SkeletalDeltaTM;

				// Add on the control point offset
				OutTargetTM.AddToTranslation(OutTargetTM.GetRotation() * Record.PhysicsControl.ControlSettings.ControlPoint);

				FQuat ParentBoneQ = ParentBoneTM.GetRotation();
				FQuat ParentBoneInvQ = ParentBoneQ.Inverse();

				if (bCalculateVelocity)
				{
					if (Record.PhysicsControl.ControlSettings.SkeletalAnimationVelocityMultiplier)
					{
						// Offset of the control point from the target child bone TM, in world space.
						FVector WorldControlPointOffset =
							ChildBoneTM.GetRotation() * Record.PhysicsControl.ControlSettings.ControlPoint;
						// World space position of the target control point
						FVector WorldChildControlPointPosition = ChildBoneTM.GetTranslation() + WorldControlPointOffset;

						// World-space velocity of the control point due to the motion of the parent
						// linear and angular velocity.
						FVector ChildTargetVelocityDueToParent =
							ParentBoneData.Velocity + ParentBoneData.AngularVelocity.Cross(
								WorldChildControlPointPosition - ParentBoneTM.GetTranslation());
						// World-space velocity of the control point due to the motion of the child
						// linear and angular velocity
						FVector ChildTargetVelocity =
							ChildBoneData.Velocity + ChildBoneData.AngularVelocity.Cross(WorldControlPointOffset);

						// Pull out just the motion in the child that isn't due to the parent
						FVector SkeletalTargetVelocity =
							ParentBoneInvQ * (ChildTargetVelocity - ChildTargetVelocityDueToParent);
						OutTargetVelocity += SkeletalTargetVelocity *
							Record.PhysicsControl.ControlSettings.SkeletalAnimationVelocityMultiplier;

						FVector SkeletalTargetAngularVelocity =
							ParentBoneInvQ * (ChildBoneData.AngularVelocity - ParentBoneData.AngularVelocity);
						OutTargetAngularVelocity += SkeletalTargetAngularVelocity *
							Record.PhysicsControl.ControlSettings.SkeletalAnimationVelocityMultiplier;
					}
				}
			}
			else
			{
				OutTargetTM = ChildBoneTM;

				// Add on the control point offset
				OutTargetTM.AddToTranslation(OutTargetTM.GetRotation()* Record.PhysicsControl.ControlSettings.ControlPoint);

				if (bCalculateVelocity)
				{
					OutTargetVelocity = ChildBoneTM.GetRotation() * OutTargetVelocity;
					OutTargetAngularVelocity = ChildBoneTM.GetRotation() * OutTargetAngularVelocity;

					if (Record.PhysicsControl.ControlSettings.SkeletalAnimationVelocityMultiplier)
					{
						FVector WorldControlPointOffset =
							ChildBoneTM.GetRotation() * Record.PhysicsControl.ControlSettings.ControlPoint;
						FVector WorldChildControlPointPosition = ChildBoneTM.GetTranslation() + WorldControlPointOffset;

						// World-space velocity of the control point due to the motion of the child
						FVector ChildTargetVelocity =
							ChildBoneData.Velocity + ChildBoneData.AngularVelocity.Cross(WorldControlPointOffset);

						OutTargetVelocity += ChildTargetVelocity *
							Record.PhysicsControl.ControlSettings.SkeletalAnimationVelocityMultiplier;

						OutTargetAngularVelocity += ChildBoneData.AngularVelocity *
							Record.PhysicsControl.ControlSettings.SkeletalAnimationVelocityMultiplier;
					}
				}
			}
		}
	}

	// Now apply the explicit target specified in the record. It operates in the space of the target
	// transform we (may have) just calculated.
	{
		const FPhysicsControlTarget& Target = Record.PhysicsControl.ControlTarget;

		// Calculate the authored target position/orientation - i.e. not using the skeletal animation
		FQuat TargetOrientationQ = Target.TargetOrientation.Quaternion();
		FVector TargetPosition = Target.TargetPosition;

		FVector ExtraTargetPosition(0);
		// Incorporate the offset from the control point. If we used animation, then we don't need
		// to do this.
		if (!bUsedSkeletalAnimation && 
			Record.PhysicsControl.ControlTarget.bApplyControlPointToTarget)
		{
			ExtraTargetPosition = TargetOrientationQ * Record.PhysicsControl.ControlSettings.ControlPoint;
		}

		// The record's target is specified in the space of the previously calculated/set OutTargetTM
		OutTargetTM = FTransform(TargetOrientationQ, TargetPosition + ExtraTargetPosition) * OutTargetTM;

		if (bCalculateVelocity)
		{
			// Note that Target.TargetAngularVelocity is in revs per second (as it's user-facing)
			FVector TargetAngularVelocity = Target.TargetAngularVelocity * UE_TWO_PI;
			OutTargetAngularVelocity += TargetAngularVelocity;
			FVector ExtraVelocity = TargetAngularVelocity.Cross(ExtraTargetPosition);
			OutTargetVelocity += Target.TargetVelocity + ExtraVelocity;
		}
	}
}

//======================================================================================================================
bool FPhysicsControlComponentImpl::ApplyControlStrengths(
	FPhysicsControlRecord& Record, FConstraintInstance* ConstraintInstance)
{
	const FPhysicsControlData& Data = Record.PhysicsControl.ControlData;
	const FPhysicsControlMultipliers& Multipliers = Record.PhysicsControl.ControlMultipliers;

	double AngularSpring;
	double AngularDamping;
	double MaxTorque = Data.MaxTorque * Multipliers.MaxTorqueMultiplier;

	FVector LinearSpring;
	FVector LinearDamping;
	FVector MaxForce = Data.MaxForce * Multipliers.MaxForceMultiplier;

	UE::PhysicsControlComponent::ConvertStrengthToSpringParams(
		AngularSpring, AngularDamping,
		Data.AngularStrength * Multipliers.AngularStrengthMultiplier,
		Data.AngularDampingRatio,
		Data.AngularExtraDamping * Multipliers.AngularExtraDampingMultiplier);
	UE::PhysicsControlComponent::ConvertStrengthToSpringParams(
		LinearSpring, LinearDamping,
		Data.LinearStrength * Multipliers.LinearStrengthMultiplier,
		Data.LinearDampingRatio,
		Data.LinearExtraDamping * Multipliers.LinearExtraDampingMultiplier);

	if (Multipliers.MaxTorqueMultiplier <= 0.0)
	{
		AngularSpring = 0.0;
		AngularDamping = 0.0;
	}
	if (Multipliers.MaxForceMultiplier.X <= 0.0)
	{
		LinearSpring.X = 0.0;
		LinearDamping.X = 0.0;
	}
	if (Multipliers.MaxForceMultiplier.Y <= 0.0)
	{
		LinearSpring.Y = 0.0;
		LinearDamping.Y = 0.0;
	}
	if (Multipliers.MaxForceMultiplier.Z <= 0.0)
	{
		LinearSpring.Z = 0.0;
		LinearDamping.Z = 0.0;
	}

	ConstraintInstance->SetAngularDriveParams(AngularSpring, AngularDamping, MaxTorque);
	ConstraintInstance->SetLinearDriveParams(LinearSpring, LinearDamping, MaxForce);

	double TestAngular = (AngularSpring + AngularDamping) * FMath::Max(UE_SMALL_NUMBER, MaxTorque);
	FVector TestLinear = (LinearSpring + LinearDamping) * FVector(
		FMath::Max(UE_SMALL_NUMBER, MaxForce.X),
		FMath::Max(UE_SMALL_NUMBER, MaxForce.Y),
		FMath::Max(UE_SMALL_NUMBER, MaxForce.Z));
	double Test = TestAngular + TestLinear.GetMax();
	return Test > 0.0;
}

//======================================================================================================================
void FPhysicsControlComponentImpl::ApplyControl(FPhysicsControlRecord& Record)
{
	FConstraintInstance* ConstraintInstance = Record.PhysicsControlState.ConstraintInstance.Get();

	if (!ConstraintInstance || !Record.PhysicsControlState.bEnabled)
	{
		return;
	}

	FBodyInstance* ParentBodyInstance = UE::PhysicsControlComponent::GetBodyInstance(
		Record.PhysicsControl.ParentMeshComponent, Record.PhysicsControl.ParentBoneName);

	FBodyInstance* ChildBodyInstance = UE::PhysicsControlComponent::GetBodyInstance(
		Record.PhysicsControl.ChildMeshComponent, Record.PhysicsControl.ChildBoneName);

	if (!ParentBodyInstance && !ChildBodyInstance)
	{
		return;
	}

	ConstraintInstance->SetDisableCollision(Record.PhysicsControl.ControlSettings.bDisableCollision);

	// Set strengths etc
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
FPhysicsBodyModifier* FPhysicsControlComponentImpl::FindBodyModifier(const FName Name)
{
	return PhysicsBodyModifiers.Find(Name);
}

