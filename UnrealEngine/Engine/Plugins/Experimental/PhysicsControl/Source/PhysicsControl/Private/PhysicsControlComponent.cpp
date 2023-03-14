// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlComponent.h"
#include "PhysicsControlComponentLog.h"
#include "PhysicsControlRecord.h"
#include "PhysicsControlComponentHelpers.h"
#include "PhysicsControlComponentImpl.h"

#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

//======================================================================================================================
UPhysicsControlComponent::UPhysicsControlComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Implementation = MakePimpl<FPhysicsControlComponentImpl>(this);

	TeleportDistanceThreshold = 300.0f;
	TeleportRotationThreshold = 0.0f;

	bShowDebugVisualization = true;
	VisualizationSizeScale = 5.0f;
	VelocityPredictionTime = 0.2f;
	MaxNumControlsOrModifiersPerName = 256;

	// ActorComponent setup
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

//======================================================================================================================
void UPhysicsControlComponent::InitializeComponent()
{
	Super::InitializeComponent();
	Implementation->ResetControls(false);
}

//======================================================================================================================
void UPhysicsControlComponent::BeginDestroy()
{
	if (Implementation)
	{
		for (TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair : Implementation->PhysicsControlRecords)
		{
			DestroyControl(PhysicsControlRecordPair.Key);
			FPhysicsControlRecord& Record = PhysicsControlRecordPair.Value;
			Record.PhysicsControlState.Reset();
		}
		Implementation->PhysicsControlRecords.Empty();

		for (TPair<FName, FPhysicsBodyModifier>& PhysicsBodyModifierPair : Implementation->PhysicsBodyModifiers)
		{
			DestroyBodyModifier(PhysicsBodyModifierPair.Key);
		}
		Implementation->PhysicsBodyModifiers.Empty();
	}
	Super::BeginDestroy();
}

//======================================================================================================================
void UPhysicsControlComponent::TickComponent(
	float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Handle control removal
	bool bRemovedOneControl = false;
	for (auto It = Implementation->PhysicsControlRecords.CreateIterator(); It; ++It)
	{
		FPhysicsControlRecord& Record = It.Value();

		if (Record.PhysicsControlState.bPendingDestroy)
		{
			Record.PhysicsControlState.Reset();
			It.RemoveCurrent();
			bRemovedOneControl = true;
		}
	}
	Implementation->PhysicsControlRecords.Compact();

	// Handle body modifier removal
	bool bRemovedOneBodyModifier = false;
	for (auto It = Implementation->PhysicsBodyModifiers.CreateIterator(); It; ++It)
	{
		FPhysicsBodyModifier& BodyModifier = It.Value();
		if (BodyModifier.bPendingDestroy)
		{
			It.RemoveCurrent();
			bRemovedOneBodyModifier = true;
		}
	}
	Implementation->PhysicsBodyModifiers.Compact();

	// We only want to continue the update if this is a "real" tick that corresponds to updating the
	// world. We certainly don't want to tick during a pause, because part of the processing involves 
	// (optionally) calculating target velocities based on target positions in previous ticks etc.
	if (TickType != LEVELTICK_All)
	{
		return;
	}

	// Update the skeletal mesh caches
	Implementation->UpdateCachedSkeletalBoneData(DeltaTime);

	for (TPair<FName, FPhysicsControlRecord>& RecordPair : Implementation->PhysicsControlRecords)
	{
		// New constraint requested when one doesn't exist
		FPhysicsControlRecord& Record = RecordPair.Value;
		FConstraintInstance* ConstraintInstance = Record.PhysicsControlState.ConstraintInstance.Get();
		if (!ConstraintInstance)
		{
			ConstraintInstance = Record.CreateConstraint(this);
		}

		if (ConstraintInstance)
		{
			// Constraint is not enabled
			if (!Record.PhysicsControlState.bEnabled)
			{
				ConstraintInstance->SetAngularDriveParams(0.0f, 0.0f, 0.0f);
				ConstraintInstance->SetLinearDriveParams(0.0f, 0.0f, 0.0f);
				ConstraintInstance->SetDisableCollision(false);
			}
			else
			{
				Implementation->ApplyControl(Record);
			}
		}
	}

	// Handle body modifiers
	for (TPair<FName, FPhysicsBodyModifier>& BodyModifierPair : Implementation->PhysicsBodyModifiers)
	{
		FPhysicsBodyModifier& BodyModifier = BodyModifierPair.Value;
		FBodyInstance* BodyInstance = UE::PhysicsControlComponent::GetBodyInstance(
			BodyModifier.MeshComponent, BodyModifier.BoneName);

		switch (BodyModifier.MovementType)
		{
		case EPhysicsMovementType::Static:
			BodyInstance->SetInstanceSimulatePhysics(false, true);
			break;
		case EPhysicsMovementType::Kinematic:
			BodyInstance->SetInstanceSimulatePhysics(false, true);
			Implementation->ApplyKinematicTarget(BodyModifier);
			break;
		case EPhysicsMovementType::Simulated:
			BodyInstance->SetInstanceSimulatePhysics(true, false, true);
			break;
		default:
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Invalid movement type %d"), BodyModifier.MovementType);
			break;
		}

		UBodySetup* BodySetup = BodyInstance->GetBodySetup();
		if (BodySetup)
		{
			int32 NumShapes = BodySetup->AggGeom.GetElementCount();
			for (int32 ShapeIndex = 0 ; ShapeIndex != NumShapes ; ++ShapeIndex)
			{
				BodyInstance->SetShapeCollisionEnabled(ShapeIndex, BodyModifier.CollisionType);
			}
		}

		if (BodyInstance->IsInstanceSimulatingPhysics())
		{
			float GravityZ = BodyInstance->GetPhysicsScene()->GetOwningWorld()->GetGravityZ();
			float AppliedGravityZ = BodyInstance->bEnableGravity ? GravityZ : 0.0f;
			float DesiredGravityZ = GravityZ * BodyModifier.GravityMultiplier;
			float GravityZToApply = DesiredGravityZ - AppliedGravityZ;
			BodyInstance->AddForce(FVector(0, 0, GravityZToApply), true, true);
		}
	}

	// Go through and de-activate any records if they're set to auto disable. 
	for (TPair<FName, FPhysicsControlRecord>& RecordPair : Implementation->PhysicsControlRecords)
	{
		FPhysicsControlRecord& Record = RecordPair.Value;
		if (Record.PhysicsControl.ControlSettings.bAutoDisable)
		{
			Record.PhysicsControlState.bEnabled = false;
		}
	}
}

//======================================================================================================================
TMap<FName, FPhysicsControlLimbBones> UPhysicsControlComponent::GetLimbBonesFromSkeletalMesh(
	USkeletalMeshComponent*                     SkeletalMeshComponent,
	const TArray<FPhysicsControlLimbSetupData>& LimbSetupDatas) const
{
	TMap<FName, FPhysicsControlLimbBones> Result;

	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	TSet<FName> AllBones;

	// Now walk through each limb, picking up bones, ignoring any that we have already encountered.
	// This requires the setup data to have been ordered properly.
	for (const FPhysicsControlLimbSetupData& LimbSetupData : LimbSetupDatas)
	{
		FPhysicsControlLimbBones& LimbBones = Result.Add(LimbSetupData.LimbName);
		LimbBones.SkeletalMeshComponent = SkeletalMeshComponent;

		if (LimbSetupData.bIncludeParentBone)
		{
			LimbBones.bFirstBoneIsAdditional = true;
			FName ParentBoneName;
			ParentBoneName = UE::PhysicsControlComponent::GetPhysicalParentBone(
				SkeletalMeshComponent, LimbSetupData.StartBone);
			if (!ParentBoneName.IsNone())
			{
				LimbBones.BoneNames.Add(ParentBoneName);
				AllBones.Add(ParentBoneName);
			}
		}
		else
		{
			LimbBones.bFirstBoneIsAdditional = false;
		}

		SkeletalMeshComponent->ForEachBodyBelow(
			LimbSetupData.StartBone, true, /*bSkipCustomType=*/false,
			[PhysicsAsset, &AllBones, &LimbBones](const FBodyInstance* BI)
			{
				if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
				{
					const FName BoneName = PhysicsAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;
					if (!AllBones.Find(BoneName))
					{
						LimbBones.BoneNames.Add(BoneName);
						AllBones.Add(BoneName);
					}
				}
			});
	}
	return Result;
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
FName UPhysicsControlComponent::MakeControl(
	UMeshComponent*         ParentMeshComponent,
	const FName             ParentBoneName,
	UMeshComponent*         ChildMeshComponent,
	const FName             ChildBoneName,
	FPhysicsControlData     ControlData, 
	FPhysicsControlTarget   ControlTarget, 
	FPhysicsControlSettings ControlSettings,
	bool                    bEnabled)
{
	FName Name = Implementation->GetUniqueControlName(ParentBoneName, ChildBoneName);
	if (MakeNamedControl(
		Name, ParentMeshComponent, ParentBoneName, ChildMeshComponent, ChildBoneName, 
		ControlData, ControlTarget, ControlSettings, bEnabled))
	{
		return Name;
	}
	return FName();
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::MakeNamedControl(
	const FName             Name, 
	UMeshComponent*         ParentMeshComponent,
	const FName             ParentBoneName,
	UMeshComponent*         ChildMeshComponent,
	const FName             ChildBoneName,
	FPhysicsControlData     ControlData, 
	FPhysicsControlTarget   ControlTarget, 
	FPhysicsControlSettings ControlSettings,
	bool                    bEnabled)
{
	FPhysicsControlRecord* PhysicsControlRecord = Implementation->FindControlRecord(Name);
	if (PhysicsControlRecord)
	{
		return false;
	}

	if (!ChildMeshComponent)
	{
		UE_LOG(LogPhysicsControlComponent, Warning,
			TEXT("Unable to make a Control as the child mesh component has not been set"));
		return false;
	}

	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ParentMeshComponent))
	{
		Implementation->AddSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
	}
	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ChildMeshComponent))
	{
		Implementation->AddSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
	}

	FPhysicsControlRecord& NewRecord = Implementation->PhysicsControlRecords.Add(
		Name, FPhysicsControl(
			ParentMeshComponent, ParentBoneName, ChildMeshComponent, ChildBoneName,
			ControlData, ControlTarget, ControlSettings));
	NewRecord.PhysicsControlState.bEnabled = bEnabled;
	NewRecord.ResetControlPoint();

	return true;
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
TArray<FName> UPhysicsControlComponent::MakeControlsFromSkeletalMeshBelow(
	USkeletalMeshComponent* SkeletalMeshComponent,
	FName                   BoneName,
	bool                    bIncludeSelf,
	EPhysicsControlType     ControlType,
	FPhysicsControlData     ControlData,
	FPhysicsControlSettings ControlSettings,
	bool                    bEnabled)
{
	TArray<FName> Result;
	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	int32 NumBones = SkeletalMeshComponent->GetNumBones();
	UMeshComponent* ParentMeshComponent = 
		(ControlType == EPhysicsControlType::ParentSpace) ? SkeletalMeshComponent : nullptr;

	SkeletalMeshComponent->ForEachBodyBelow(
		BoneName, bIncludeSelf, /*bSkipCustomType=*/false, 
		[
			this, PhysicsAsset, ParentMeshComponent, SkeletalMeshComponent, 
			&ControlData, &ControlSettings, &Result, bEnabled
		](const FBodyInstance* BI)
		{
			if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
			{
				const FName ChildBoneName = PhysicsAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;

				FName ParentBoneName;
				if (ParentMeshComponent)
				{
					ParentBoneName = UE::PhysicsControlComponent::GetPhysicalParentBone(
						SkeletalMeshComponent, ChildBoneName);
					if (ParentBoneName.IsNone())
					{
						return;
					}
				}
				FName ControlName = MakeControl(
					ParentMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
					ControlData, FPhysicsControlTarget(), ControlSettings, bEnabled);
				if (!ControlName.IsNone())
				{
					Result.Add(ControlName);
				}
				else
				{
					UE_LOG(LogPhysicsControlComponent, Warning, 
						TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
				}
			}
		});

	return Result;
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
TArray<FName> UPhysicsControlComponent::MakeControlsFromSkeletalMeshAndConstraintProfileBelow(
	USkeletalMeshComponent* SkeletalMeshComponent,
	FName                   BoneName,
	bool                    bIncludeSelf,
	FName                   ConstraintProfile,
	bool                    bEnabled)
{
	TArray<FName> Result;
	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	FPhysicsControlSettings ControlSettings;
	// This is to match the skeletal mesh component velocity drive, which does not use the
	// target animation velocity.
	ControlSettings.SkeletalAnimationVelocityMultiplier = 0;

	SkeletalMeshComponent->ForEachBodyBelow(
		BoneName, bIncludeSelf, /*bSkipCustomType=*/false,
		[
			this, PhysicsAsset, SkeletalMeshComponent,
			ConstraintProfile, &ControlSettings, &Result, bEnabled
		](const FBodyInstance* BI)
		{
			if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
			{
				const FName ChildBoneName = PhysicsAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;

				FName ParentBoneName;
				ParentBoneName = UE::PhysicsControlComponent::GetPhysicalParentBone(
					SkeletalMeshComponent, ChildBoneName);
				if (ParentBoneName.IsNone())
				{
					return;
				}

				FPhysicsControlData ControlData;
				FConstraintProfileProperties ProfileProperties;
				if (!SkeletalMeshComponent->GetConstraintProfilePropertiesOrDefault(
					ProfileProperties, ChildBoneName, ConstraintProfile))
				{
					UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
					return;
				}

				UE::PhysicsControlComponent::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);

				FName ControlName = MakeControl(
					SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
					ControlData, FPhysicsControlTarget(), ControlSettings, bEnabled);
				if (!ControlName.IsNone())
				{
					Result.Add(ControlName);
				}
				else
				{
					UE_LOG(LogPhysicsControlComponent, Warning,
						TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
				}
			}
		});

	return Result;
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
TArray<FName> UPhysicsControlComponent::MakeControlsFromSkeletalMesh(
	USkeletalMeshComponent* SkeletalMeshComponent,
	const TArray<FName>&    BoneNames,
	EPhysicsControlType     ControlType,
	FPhysicsControlData     ControlData,
	FPhysicsControlSettings ControlSettings,
	bool                    bEnabled)
{
	TArray<FName> Result;
	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	UMeshComponent* ParentMeshComponent =
		(ControlType == EPhysicsControlType::ParentSpace) ? SkeletalMeshComponent : nullptr;

	for (FName ChildBoneName : BoneNames)
	{
		FBodyInstance* ChildBone = SkeletalMeshComponent->GetBodyInstance(ChildBoneName);

		FName ParentBoneName;
		if (ParentMeshComponent)
		{
			ParentBoneName = UE::PhysicsControlComponent::GetPhysicalParentBone(
				SkeletalMeshComponent, ChildBoneName);
			if (ParentBoneName.IsNone())
			{
				continue;
			}
		}
		FName ControlName = MakeControl(
			ParentMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
			ControlData, FPhysicsControlTarget(), ControlSettings, bEnabled);
		if (!ControlName.IsNone())
		{
			Result.Add(ControlName);
		}
		else
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
		}
	}

	return Result;
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
TArray<FName> UPhysicsControlComponent::MakeControlsFromSkeletalMeshAndConstraintProfile(
	USkeletalMeshComponent* SkeletalMeshComponent,
	const TArray<FName>&    BoneNames,
	FName                   ConstraintProfile,
	bool                    bEnabled)
{
	TArray<FName> Result;
	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	FPhysicsControlSettings ControlSettings;
	// This is to match the skeletal mesh component velocity drive, which does not use the
	// target animation velocity.
	ControlSettings.SkeletalAnimationVelocityMultiplier = 0;

	for (FName ChildBoneName : BoneNames)
	{
		FBodyInstance* ChildBone = SkeletalMeshComponent->GetBodyInstance(ChildBoneName);

		FName ParentBoneName;
		ParentBoneName = UE::PhysicsControlComponent::GetPhysicalParentBone(SkeletalMeshComponent, ChildBoneName);
		if (ParentBoneName.IsNone())
		{
			continue;
		}

		FPhysicsControlData ControlData;
		FConstraintProfileProperties ProfileProperties;
		if (!SkeletalMeshComponent->GetConstraintProfilePropertiesOrDefault(
			ProfileProperties, ChildBoneName, ConstraintProfile))
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
			continue;
		}

		UE::PhysicsControlComponent::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);

		FName ControlName = MakeControl(
			SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
			ControlData, FPhysicsControlTarget(), ControlSettings, bEnabled);
		if (!ControlName.IsNone())
		{
			Result.Add(ControlName);
		}
		else
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
		}
	}

	return Result;
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
TMap<FName, FPhysicsControlNameArray> UPhysicsControlComponent::MakeControlsFromLimbBones(
	FPhysicsControlNameArray&                    AllControls,
	const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
	EPhysicsControlType                          ControlType,
	FPhysicsControlData                          ControlData,
	FPhysicsControlSettings                      ControlSettings,
	bool                                         bEnabled)
{
	TMap<FName, FPhysicsControlNameArray> Result;
	Result.Reserve(LimbBones.Num());

	for (const TPair< FName, FPhysicsControlLimbBones>& LimbBoneEntry : LimbBones)
	{
		FName LimbName = LimbBoneEntry.Key;
		const FPhysicsControlLimbBones& BonesInLimb = LimbBoneEntry.Value;

		if (!BonesInLimb.SkeletalMeshComponent)
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No Skeletal mesh in limb %s"), *LimbName.ToString());
			continue;
		}

		USkeletalMeshComponent* ParentMeshComponent =
			(ControlType == EPhysicsControlType::ParentSpace) ? BonesInLimb.SkeletalMeshComponent : nullptr;

		const int32 NumBonesInLimb = BonesInLimb.BoneNames.Num();

		FPhysicsControlNameArray& LimbResult = Result.Add(LimbName);
		LimbResult.Names.Reserve(NumBonesInLimb);
		AllControls.Names.Reserve(AllControls.Names.Num() + NumBonesInLimb);

		for (int32 BoneIndex = 0 ; BoneIndex != NumBonesInLimb ; ++BoneIndex)
		{
			// Don't create the parent space control if it's the first bone in a limb that had bIncludeParentBone
			if (BoneIndex == 0 && BonesInLimb.bFirstBoneIsAdditional && ControlType == EPhysicsControlType::ParentSpace)
			{
				continue;
			}

			FName ChildBoneName = BonesInLimb.BoneNames[BoneIndex];
			FBodyInstance* ChildBone = BonesInLimb.SkeletalMeshComponent->GetBodyInstance(ChildBoneName);

			FName ParentBoneName;
			if (ParentMeshComponent)
			{
				ParentBoneName = UE::PhysicsControlComponent::GetPhysicalParentBone(
					ParentMeshComponent, ChildBoneName);
				if (ParentBoneName.IsNone())
				{
					continue;
				}
			}
			FName ControlName = MakeControl(
				ParentMeshComponent, ParentBoneName, BonesInLimb.SkeletalMeshComponent, ChildBoneName,
				ControlData, FPhysicsControlTarget(), ControlSettings, bEnabled);
			if (!ControlName.IsNone())
			{
				LimbResult.Names.Add(ControlName);
				AllControls.Names.Add(ControlName);
			}
			else
			{
				UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
			}
		}
	}
	return Result;
}

//======================================================================================================================
TMap<FName, FPhysicsControlNameArray> UPhysicsControlComponent::MakeControlsFromLimbBonesAndConstraintProfile(
	FPhysicsControlNameArray&                    AllControls,
	const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
	FName                                        ConstraintProfile,
	bool                                         bEnabled)
{
	TMap<FName, FPhysicsControlNameArray> Result;
	Result.Reserve(LimbBones.Num());
	for (const TPair< FName, FPhysicsControlLimbBones>& LimbBoneEntry : LimbBones)
	{
		FName LimbName = LimbBoneEntry.Key;
		const FPhysicsControlLimbBones& BonesInLimb = LimbBoneEntry.Value;

		USkeletalMeshComponent* SkeletalMeshComponent = BonesInLimb.SkeletalMeshComponent;
		if (!SkeletalMeshComponent)
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No Skeletal mesh in limb %s"), *LimbName.ToString());
			continue;
		}
		UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
		if (!PhysicsAsset)
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No physics asset in skeletal mesh"));
			return Result;
		}

		const int32 NumBonesInLimb = BonesInLimb.BoneNames.Num();

		FPhysicsControlNameArray& LimbResult = Result.Add(LimbName);
		LimbResult.Names.Reserve(NumBonesInLimb);
		AllControls.Names.Reserve(AllControls.Names.Num() + NumBonesInLimb);

		FPhysicsControlSettings ControlSettings;
		// This is to match the skeletal mesh component velocity drive, which does not use the
		// target animation velocity.
		ControlSettings.SkeletalAnimationVelocityMultiplier = 0;

		for (int32 BoneIndex = 0; BoneIndex != NumBonesInLimb; ++BoneIndex)
		{
			// Don't create the parent space control if it's the first bone in a limb that had bIncludeParentBone
			if (BoneIndex == 0 && BonesInLimb.bFirstBoneIsAdditional)
			{
				continue;
			}

			FName ChildBoneName = BonesInLimb.BoneNames[BoneIndex];
			FBodyInstance* ChildBone = SkeletalMeshComponent->GetBodyInstance(ChildBoneName);
			FName ParentBoneName;
			ParentBoneName = UE::PhysicsControlComponent::GetPhysicalParentBone(SkeletalMeshComponent, ChildBoneName);
			if (ParentBoneName.IsNone())
			{
				continue;
			}

			FPhysicsControlData ControlData;
			FConstraintProfileProperties ProfileProperties;
			if (!SkeletalMeshComponent->GetConstraintProfilePropertiesOrDefault(
				ProfileProperties, ChildBoneName, ConstraintProfile))
			{
				UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
				continue;
			}

			UE::PhysicsControlComponent::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);

			FName ControlName = MakeControl(
				SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
				ControlData, FPhysicsControlTarget(), ControlSettings, bEnabled);
			if (!ControlName.IsNone())
			{
				LimbResult.Names.Add(ControlName);
				AllControls.Names.Add(ControlName);
			}
			else
			{
				UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
			}
		}
	}
	return Result;
}

//======================================================================================================================
bool UPhysicsControlComponent::DestroyControl(const FName Name)
{
	FPhysicsControlRecord* PhysicsControlRecord = Implementation->FindControlRecord(Name);
	if (PhysicsControlRecord)
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = 
			Cast<USkeletalMeshComponent>(PhysicsControlRecord->PhysicsControl.ParentMeshComponent))
		{
			Implementation->RemoveSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		}
		if (USkeletalMeshComponent* SkeletalMeshComponent =
			Cast<USkeletalMeshComponent>(PhysicsControlRecord->PhysicsControl.ChildMeshComponent))
		{
			Implementation->RemoveSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		}

		PhysicsControlRecord->PhysicsControlState.bPendingDestroy = true;
		PhysicsControlRecord->PhysicsControlState.bEnabled = false;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyAllControls(const TArray<FName>& Names)
{
	for (FName Name : Names)
	{
		DestroyControl(Name);
	}
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlEnabled(const FName Name, bool bEnable)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControlState.bEnabled = bEnable;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllControlsEnabled(const TArray<FName>& Names, bool bEnable)
{
	for (FName Name : Names)
	{
		SetControlEnabled(Name, bEnable);
	}
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetControlData(
	const FName         Name, 
	FPhysicsControlData ControlData, 
	bool                bEnableControl)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData = ControlData;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllControlDatas(
	const TArray<FName>& Names, 
	FPhysicsControlData  ControlData, 
	bool                 bEnableControl)
{
	for (FName Name : Names)
	{
		SetControlData(Name, ControlData, bEnableControl);
	}
}


//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetControlMultipliers(
	const FName                Name, 
	FPhysicsControlMultipliers ControlMultipliers, 
	bool                       bEnableControl)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlMultipliers = ControlMultipliers;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllControlMultipliers(
	const TArray<FName>&       Names,
	FPhysicsControlMultipliers ControlMultipliers, 
	bool                       bEnableControl)
{
	for (FName Name : Names)
	{
		SetControlMultipliers(Name, ControlMultipliers, bEnableControl);
	}
}


//======================================================================================================================
bool UPhysicsControlComponent::SetControlLinearData(
	const FName Name, float Strength, float DampingRatio, float ExtraDamping, float MaxForce, bool bEnableControl)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData.LinearStrength = Strength;
		Record->PhysicsControl.ControlData.LinearDampingRatio = DampingRatio;
		Record->PhysicsControl.ControlData.LinearExtraDamping = ExtraDamping;
		Record->PhysicsControl.ControlData.MaxForce = MaxForce;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlAngularData(
	const FName Name, float Strength, float DampingRatio, float ExtraDamping, float MaxTorque, bool bEnableControl)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData.AngularStrength = Strength;
		Record->PhysicsControl.ControlData.AngularDampingRatio = DampingRatio;
		Record->PhysicsControl.ControlData.AngularExtraDamping = ExtraDamping;
		Record->PhysicsControl.ControlData.MaxTorque = MaxTorque;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlPoint(const FName Name, const FVector Position)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlSettings.ControlPoint = Position;
		Record->UpdateConstraintControlPoint();
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::ResetControlPoint(const FName Name)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->ResetControlPoint();
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTarget(
	const FName           Name, 
	FPhysicsControlTarget ControlTarget, 
	bool                  bEnableControl)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlTarget = ControlTarget;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllControlTargets(
	const TArray<FName>&  Names, 
	FPhysicsControlTarget ControlTarget, 
	bool                  bEnableControl)
{
	for (FName Name : Names)
	{
		SetControlTarget(Name, ControlTarget, bEnableControl);
	}
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetPositionAndOrientation(
	const FName Name, const FVector Position, const FRotator Orientation, float VelocityDeltaTime, 
	bool bEnableControl, bool bApplyControlPointToTarget)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		SetControlTargetPosition(
			Name, Position, VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
		SetControlTargetOrientation(
			Name, Orientation, VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
	}
	return false;

}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetPosition(
	const FName Name, const FVector Position, float VelocityDeltaTime, 
	bool bEnableControl, bool bApplyControlPointToTarget)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		if (VelocityDeltaTime)
		{
			Record->PhysicsControl.ControlTarget.TargetVelocity =
				(Position - Record->PhysicsControl.ControlTarget.TargetPosition) / VelocityDeltaTime;
		}
		else
		{
			Record->PhysicsControl.ControlTarget.TargetVelocity = FVector::ZeroVector;
		}
		Record->PhysicsControl.ControlTarget.TargetPosition = Position;
		Record->PhysicsControl.ControlTarget.bApplyControlPointToTarget = bApplyControlPointToTarget;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetOrientation(
	const FName Name, const FRotator Orientation, float AngularVelocityDeltaTime, 
	bool bEnableControl, bool bApplyControlPointToTarget)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		if (AngularVelocityDeltaTime)
		{
			FQuat OldQ = Record->PhysicsControl.ControlTarget.TargetOrientation.Quaternion();
			FQuat OrientationQ = Orientation.Quaternion();
			OldQ.EnforceShortestArcWith(OrientationQ);
			// Note that quats multiply in the opposite order to TMs
			FQuat DeltaQ = OrientationQ * OldQ.Inverse();
			Record->PhysicsControl.ControlTarget.TargetAngularVelocity =
				DeltaQ.ToRotationVector() / (UE_TWO_PI * AngularVelocityDeltaTime);
		}
		else
		{
			Record->PhysicsControl.ControlTarget.TargetAngularVelocity = FVector::ZeroVector;
		}
		Record->PhysicsControl.ControlTarget.TargetOrientation = Orientation;
		Record->PhysicsControl.ControlTarget.bApplyControlPointToTarget = bApplyControlPointToTarget;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetPoses(
	const FName Name,
	const FVector ParentPosition, const FRotator ParentOrientation,
	const FVector ChildPosition, const FRotator ChildOrientation,
	float VelocityDeltaTime, bool bEnableControl)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		FTransform ParentTM(ParentOrientation, ParentPosition, FVector::One());
		FTransform ChildTM(ChildOrientation, ChildPosition, FVector::One());

		FTransform OffsetTM = ChildTM * ParentTM.Inverse();
		FVector Position = OffsetTM.GetTranslation();
		FQuat OrientationQ = OffsetTM.GetRotation();

		if (VelocityDeltaTime)
		{
			FQuat OldQ = Record->PhysicsControl.ControlTarget.TargetOrientation.Quaternion();
			OldQ.EnforceShortestArcWith(OrientationQ);
			// Note that quats multiply in the opposite order to TMs
			FQuat DeltaQ = OrientationQ * OldQ.Inverse();
			Record->PhysicsControl.ControlTarget.TargetAngularVelocity =
				DeltaQ.ToRotationVector() / (UE_TWO_PI * VelocityDeltaTime);

			Record->PhysicsControl.ControlTarget.TargetVelocity =
				(Position - Record->PhysicsControl.ControlTarget.TargetPosition) / VelocityDeltaTime;
		}
		else
		{
			Record->PhysicsControl.ControlTarget.TargetAngularVelocity = FVector::ZeroVector;
			Record->PhysicsControl.ControlTarget.TargetVelocity = FVector::ZeroVector;
		}
		Record->PhysicsControl.ControlTarget.TargetOrientation = OrientationQ.Rotator();
		Record->PhysicsControl.ControlTarget.TargetPosition = Position;
		Record->PhysicsControl.ControlTarget.bApplyControlPointToTarget = true;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlUseSkeletalAnimation(
	const FName Name,
	bool        bUseSkeletalAnimation,
	float       SkeletalAnimationVelocityMultiplier)
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		PhysicsControl->ControlSettings.bUseSkeletalAnimation = bUseSkeletalAnimation;
		PhysicsControl->ControlSettings.SkeletalAnimationVelocityMultiplier = SkeletalAnimationVelocityMultiplier;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllControlsUseSkeletalAnimation(
	const TArray<FName>& Names,
	bool                 bUseSkeletalAnimation,
	float                SkeletalAnimationVelocityMultiplier)
{
	for (FName Name : Names)
	{
		SetControlUseSkeletalAnimation(Name, bUseSkeletalAnimation, SkeletalAnimationVelocityMultiplier);
	}
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlAutoDisable(const FName Name, bool bAutoDisable)
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		PhysicsControl->ControlSettings.bAutoDisable = bAutoDisable;
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControl(const FName Name, FPhysicsControl& Control) const
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		Control = *PhysicsControl;
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControlData(const FName Name, FPhysicsControlData& ControlData) const
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		ControlData = PhysicsControl->ControlData;
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControlMultipliers(const FName Name, FPhysicsControlMultipliers& ControlMultipliers) const
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		ControlMultipliers = PhysicsControl->ControlMultipliers;
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControlTarget(const FName Name, FPhysicsControlTarget& ControlTarget) const
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		ControlTarget = PhysicsControl->ControlTarget;
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControlAutoDisable(const FName Name) const
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		return PhysicsControl->ControlSettings.bAutoDisable;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControlEnabled(const FName Name) const
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		return Record->PhysicsControlState.bEnabled;
	}
	return false;
}

//======================================================================================================================
FName UPhysicsControlComponent::MakeBodyModifier(
	UMeshComponent*         MeshComponent,
	const FName             BoneName,
	EPhysicsMovementType    MovementType, 
	ECollisionEnabled::Type CollisionType,
	float                   GravityMultiplier,
	bool                    bUseSkeletalAnimation)
{
	FName Name = Implementation->GetUniqueBodyModifierName(BoneName);
	if (MakeNamedBodyModifier(
		Name, MeshComponent, BoneName, MovementType, CollisionType, GravityMultiplier, bUseSkeletalAnimation))
	{
		return Name;
	}
	return FName();
}

//======================================================================================================================
bool UPhysicsControlComponent::MakeNamedBodyModifier(
	const FName             Name,
	UMeshComponent*         MeshComponent,
	const FName             BoneName,
	EPhysicsMovementType    MovementType,
	ECollisionEnabled::Type CollisionType,
	float                   GravityMultiplier,
	bool                    bUseSkeletalAnimation)
{
	FPhysicsBodyModifier* BodyModifier = Implementation->FindBodyModifier(Name);
	if (BodyModifier)
	{
		return false;
	}

	if (!MeshComponent)
	{
		UE_LOG(LogPhysicsControlComponent, Warning,
			TEXT("Unable to make a PhysicsBodyModifier as the mesh component has not been set"));
		return false;
	}

	FPhysicsBodyModifier& Modifier = Implementation->PhysicsBodyModifiers.Add(
		Name, FPhysicsBodyModifier(
			MeshComponent, BoneName, MovementType, CollisionType, GravityMultiplier, bUseSkeletalAnimation));

	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent);
	if (SkeletalMeshComponent)
	{
		Implementation->AddSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		Implementation->AddSkeletalMeshReferenceForModifier(SkeletalMeshComponent);
	}

	return true;
}

//======================================================================================================================
TArray<FName> UPhysicsControlComponent::MakeBodyModifiersFromSkeletalMeshBelow(
	USkeletalMeshComponent* SkeletalMeshComponent,
	FName                   BoneName,
	bool                    bIncludeSelf,
	EPhysicsMovementType    MovementType,
	ECollisionEnabled::Type CollisionType,
	float                   GravityMultiplier,
	bool                    bUseSkeletalAnimation)
{
	TArray<FName> Result;
	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		return Result;
	}

	SkeletalMeshComponent->ForEachBodyBelow(
		BoneName, bIncludeSelf, /*bSkipCustomType=*/false,
		[
			this, PhysicsAsset, SkeletalMeshComponent, MovementType, CollisionType,
			GravityMultiplier, bUseSkeletalAnimation, &Result
		](const FBodyInstance* BI)
		{
			if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
			{
				const FName BoneName = PhysicsAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;
				FName BodyModifierName = MakeBodyModifier(
					SkeletalMeshComponent, BoneName, MovementType, CollisionType, GravityMultiplier, bUseSkeletalAnimation);
				Result.Add(BodyModifierName);
			}
		});

	return Result;
}

//======================================================================================================================
TMap<FName, FPhysicsControlNameArray> UPhysicsControlComponent::MakeBodyModifiersFromLimbBones(
	FPhysicsControlNameArray&                    AllBodyModifiers,
	const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
	EPhysicsMovementType                         MovementType,
	ECollisionEnabled::Type                      CollisionType,
	float                                        GravityMultiplier,
	bool                                         bUseSkeletalAnimation)
{
	TMap<FName, FPhysicsControlNameArray> Result;
	Result.Reserve(LimbBones.Num());

	for (const TPair<FName, FPhysicsControlLimbBones>& LimbBoneEntry : LimbBones)
	{
		FName LimbName = LimbBoneEntry.Key;
		const FPhysicsControlLimbBones& BonesInLimb = LimbBoneEntry.Value;

		if (!BonesInLimb.SkeletalMeshComponent)
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No Skeletal mesh in limb %s"), *LimbName.ToString());
			continue;
		}

		const int32 NumBonesInLimb = BonesInLimb.BoneNames.Num();

		FPhysicsControlNameArray& LimbResult = Result.Add(LimbName);
		LimbResult.Names.Reserve(NumBonesInLimb);
		AllBodyModifiers.Names.Reserve(AllBodyModifiers.Names.Num() + NumBonesInLimb);

		for (FName BoneName : BonesInLimb.BoneNames)
		{
			FName BodyModifierName = MakeBodyModifier(
				BonesInLimb.SkeletalMeshComponent, BoneName, MovementType, CollisionType, GravityMultiplier, bUseSkeletalAnimation);
			if (!BodyModifierName.IsNone())
			{
				LimbResult.Names.Add(BodyModifierName);
				AllBodyModifiers.Names.Add(BodyModifierName);
			}
			else
			{
				UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Failed to make body modifier for %s"), *BoneName.ToString());
			}
		}
	}
	return Result;
}

//======================================================================================================================
bool UPhysicsControlComponent::DestroyBodyModifier(const FName Name)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = 
			Cast<USkeletalMeshComponent>(PhysicsBodyModifier->MeshComponent))
		{
			Implementation->RemoveSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
			Implementation->RemoveSkeletalMeshReferenceForModifier(SkeletalMeshComponent);
		}

		PhysicsBodyModifier->bPendingDestroy = true;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyAllBodyModifiers(const TArray<FName>& Names)
{
	for (FName Name : Names)
	{
		DestroyBodyModifier(Name);
	}
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierKinematicTarget(
	const FName Name,
	FVector     KinematicTargetPosition,
	FRotator    KinematicTargetOrienation,
	bool        bMakeKinematic)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		PhysicsBodyModifier->KinematicTargetPosition = KinematicTargetPosition;
		PhysicsBodyModifier->KinematicTargetOrientation = KinematicTargetOrienation.Quaternion();
		if (bMakeKinematic)
		{
			PhysicsBodyModifier->MovementType = EPhysicsMovementType::Kinematic;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierMovementType(
		FName                Name,
		EPhysicsMovementType MovementType)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		PhysicsBodyModifier->MovementType = MovementType;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllBodyModifierMovementType(
	const TArray<FName>& Names,
	EPhysicsMovementType MovementType)
{
	for (FName Name : Names)
	{
		SetBodyModifierMovementType(Name, MovementType);
	}
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierCollisionType(
	FName                   Name,
	ECollisionEnabled::Type CollisionType)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		PhysicsBodyModifier->CollisionType = CollisionType;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllBodyModifierCollisionType(
	const TArray<FName>&    Names,
	ECollisionEnabled::Type CollisionType)
{
	for (FName Name : Names)
	{
		SetBodyModifierCollisionType(Name, CollisionType);
	}
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierGravityMultiplier(
	const FName Name,
	float       GravityMultiplier)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		PhysicsBodyModifier->GravityMultiplier = GravityMultiplier;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllBodyModifierGravityMultipliers(
	const TArray<FName>& Names,
	float                GravityMultiplier)
{
	for (FName Name : Names)
	{
		SetBodyModifierGravityMultiplier(Name, GravityMultiplier);
	}
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierUseSkeletalAnimation(
	const FName Name,
	bool        bUseSkeletalAnimation)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		PhysicsBodyModifier->bUseSkeletalAnimation = bUseSkeletalAnimation;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetAllBodyModifiersUseSkeletalAnimation(
	const TArray<FName>& Names,
	bool                 bUseSkeletalAnimation)
{
	for (FName Name : Names)
	{
		SetBodyModifierUseSkeletalAnimation(Name, bUseSkeletalAnimation);
	}
}

#if WITH_EDITOR
//======================================================================================================================
void UPhysicsControlComponent::DebugDraw(FPrimitiveDrawInterface* PDI) const
{
	if (bShowDebugVisualization && VisualizationSizeScale > 0)
	{
		for (const TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair : 
			Implementation->PhysicsControlRecords)
		{
			const FName& Name = PhysicsControlRecordPair.Key;
			const FPhysicsControlRecord& Record = PhysicsControlRecordPair.Value;
			DebugDrawControl(PDI, Record, Name);
		} 
	}
}

//======================================================================================================================
void UPhysicsControlComponent::DebugDrawControl(
	FPrimitiveDrawInterface* PDI, const FPhysicsControlRecord& Record, FName ControlName) const
{
	const float GizmoWidthScale = 0.02f * VisualizationSizeScale;
	const FColor CurrentToTargetColor(255, 0, 0);
	const FColor TargetColor(0, 255, 0);
	const FColor CurrentColor(0, 0, 255);

	const FConstraintInstance* ConstraintInstance = Record.PhysicsControlState.ConstraintInstance.Get();

	bool bHaveLinear = Record.PhysicsControl.ControlData.LinearStrength > 0;
	bool bHaveAngular = Record.PhysicsControl.ControlData.AngularStrength > 0;

	if (Record.PhysicsControlState.bEnabled && ConstraintInstance)
	{
		FBodyInstance* ChildBodyInstance = UE::PhysicsControlComponent::GetBodyInstance(
			Record.PhysicsControl.ChildMeshComponent, Record.PhysicsControl.ChildBoneName);
		if (!ChildBodyInstance)
		{
			return;
		}
		FTransform ChildBodyTM = ChildBodyInstance->GetUnrealWorldTransform();

		FBodyInstance* ParentBodyInstance = UE::PhysicsControlComponent::GetBodyInstance(
			Record.PhysicsControl.ParentMeshComponent, Record.PhysicsControl.ParentBoneName);
		FTransform ParentBodyTM = ParentBodyInstance ? ParentBodyInstance->GetUnrealWorldTransform() : FTransform();

		FTransform TargetTM;
		FVector TargetVelocity;
		FVector TargetAngularVelocity;
		Implementation->CalculateControlTargetData(TargetTM, TargetVelocity, TargetAngularVelocity, Record, true);

		// WorldChildFrameTM is the world-space transform of the child (driven) constraint frame
		FTransform WorldChildFrameTM = ConstraintInstance->GetRefFrame(EConstraintFrame::Frame1) * ChildBodyTM;

		// WorldParentFrameTM is the world-space transform of the parent constraint frame
		FTransform WorldParentFrameTM = ConstraintInstance->GetRefFrame(EConstraintFrame::Frame2) * ParentBodyTM;

		FTransform WorldTargetTM = TargetTM * WorldParentFrameTM;
		FTransform WorldCurrentTM = WorldChildFrameTM;

		if (!bHaveLinear)
		{
			WorldTargetTM.SetTranslation(WorldCurrentTM.GetTranslation());
		}
		if (!bHaveAngular)
		{
			WorldTargetTM.SetRotation(WorldCurrentTM.GetRotation());
		}

		FVector WorldTargetVelocity = WorldParentFrameTM.GetRotation() * TargetVelocity;
		FVector WorldTargetAngularVelocity = WorldParentFrameTM.GetRotation() * TargetAngularVelocity;

		// Indicate the velocities by predicting the TargetTM
		FTransform PredictedTargetTM = WorldTargetTM;
		PredictedTargetTM.AddToTranslation(WorldTargetVelocity * VelocityPredictionTime);

		// Draw the target and current positions/orientations
		if (bHaveAngular)
		{
			FQuat AngularVelocityQ = FQuat::MakeFromRotationVector(WorldTargetAngularVelocity * VelocityPredictionTime);
			PredictedTargetTM.SetRotation(AngularVelocityQ * WorldTargetTM.GetRotation());

			DrawCoordinateSystem(
				PDI, WorldCurrentTM.GetTranslation(), WorldCurrentTM.Rotator(), 
				VisualizationSizeScale, SDPG_Foreground, 1.0f * GizmoWidthScale);
			DrawCoordinateSystem(
				PDI, WorldTargetTM.GetTranslation(), WorldTargetTM.Rotator(),
				VisualizationSizeScale, SDPG_Foreground, 4.0f * GizmoWidthScale);
			if (VelocityPredictionTime)
			{
				DrawCoordinateSystem(
					PDI, PredictedTargetTM.GetTranslation(), PredictedTargetTM.Rotator(),
					VisualizationSizeScale * 0.5, SDPG_Foreground, 4.0f * GizmoWidthScale);
			}
		}
		else
		{
			DrawWireSphere(
				PDI, WorldCurrentTM, CurrentColor, 
				VisualizationSizeScale, 8, SDPG_Foreground, 1.0f * GizmoWidthScale);
			DrawWireSphere(
				PDI, WorldTargetTM, TargetColor, 
				VisualizationSizeScale, 8, SDPG_Foreground, 3.0f * GizmoWidthScale);
			if (VelocityPredictionTime)
			{
				DrawWireSphere(
					PDI, PredictedTargetTM, TargetColor, 
					VisualizationSizeScale * 0.5, 8, SDPG_Foreground, 3.0f * GizmoWidthScale);
			}
		}


		if (VelocityPredictionTime)
		{
			PDI->DrawLine(
				WorldTargetTM.GetTranslation(), 
				WorldTargetTM.GetTranslation() + WorldTargetVelocity * VelocityPredictionTime, 
				TargetColor, SDPG_Foreground);
		}

		// Connect current to target
		DrawDashedLine(
			PDI,
			WorldTargetTM.GetTranslation(), WorldCurrentTM.GetTranslation(), 
			CurrentToTargetColor, VisualizationSizeScale * 0.2f, SDPG_Foreground);
	}
}

#endif

