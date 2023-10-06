// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlComponent.h"
#include "PhysicsControlComponentLog.h"
#include "PhysicsControlRecord.h"
#include "PhysicsControlComponentHelpers.h"
#include "PhysicsControlComponentImpl.h"

#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Physics/PhysicsInterfaceCore.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BillboardComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"

#include "SceneManagement.h"

DECLARE_CYCLE_STAT(TEXT("PhysicsControl UpdateTargetCaches"), STAT_PhysicsControl_UpdateTargetCaches, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("PhysicsControl UpdateControls"), STAT_PhysicsControl_UpdateControls, STATGROUP_Anim);

//======================================================================================================================
// This is used, rather than UEnum::GetValueAsString, so that we have more control over the string returned, which 
// gets used as a prefix for the automatically named sets etc
static FName GetControlTypeName(EPhysicsControlType ControlType)
{
	switch (ControlType)
	{
	case EPhysicsControlType::ParentSpace:
		return "ParentSpace";
	case EPhysicsControlType::WorldSpace:
		return "WorldSpace";
	default:
		return "None";
	}
}

//======================================================================================================================
UPhysicsControlComponent::UPhysicsControlComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Implementation = MakePimpl<FPhysicsControlComponentImpl>(this);

	// ActorComponent setup
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif
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
			Implementation->DestroyControl(
				PhysicsControlRecordPair.Key, FPhysicsControlComponentImpl::EDestroyBehavior::KeepRecord);
		}
		Implementation->PhysicsControlRecords.Empty();

		for (TPair<FName, FPhysicsBodyModifier>& PhysicsBodyModifierPair : Implementation->PhysicsBodyModifiers)
		{
			Implementation->DestroyBodyModifier(
				PhysicsBodyModifierPair.Key, FPhysicsControlComponentImpl::EDestroyBehavior::KeepRecord);
		}
		Implementation->PhysicsBodyModifiers.Empty();
	}
	Super::BeginDestroy();
}

//======================================================================================================================
void UPhysicsControlComponent::UpdateTargetCaches(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsControl_UpdateTargetCaches);
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysicsControlComponent::UpdateTargetCaches);

	// Update the skeletal mesh caches
	Implementation->UpdateCachedSkeletalBoneData(DeltaTime);
}

//======================================================================================================================
void UPhysicsControlComponent::UpdateControls(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsControl_UpdateControls);
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysicsControlComponent::UpdateControls);

	// Tidy up
	Implementation->PhysicsControlRecords.Compact();
	Implementation->PhysicsBodyModifiers.Compact();

	for (TPair<FName, FPhysicsControlRecord>& RecordPair : Implementation->PhysicsControlRecords)
	{
		// New constraint requested when one doesn't exist
		FName ControlName = RecordPair.Key;
		FPhysicsControlRecord& Record = RecordPair.Value;
		FConstraintInstance* ConstraintInstance = Record.PhysicsControlState.ConstraintInstance.Get();
		if (!ConstraintInstance)
		{
			ConstraintInstance = Record.CreateConstraint(this, ControlName);
		}

		if (ConstraintInstance)
		{
			// Always control collision, because otherwise maintaining it is very difficult, since
			// constraint-controlled collision doesn't interact nicely when there are multiple constraints.
			ConstraintInstance->SetDisableCollision(Record.PhysicsControl.ControlSettings.bDisableCollision);

			// Constraint is not enabled
			if (!Record.PhysicsControlState.bEnabled)
			{
				ConstraintInstance->SetAngularDriveParams(0.0f, 0.0f, 0.0f);
				ConstraintInstance->SetLinearDriveParams(0.0f, 0.0f, 0.0f);
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
			BodyInstance->SetInstanceSimulatePhysics(true, true, true);
			break;
		default:
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("Invalid movement type %d"), int(BodyModifier.MovementType));
			break;
		}
		// We always overwrite the physics blend weight, since the functions above can still modify
		// it (even though they all use the "maintain physics blending" option), since there is an
		// expectation that zero blend weight means to disable physics.
		BodyInstance->PhysicsBlendWeight = BodyModifier.PhysicsBlendWeight;

		BodyInstance->SetUpdateKinematicFromSimulation(BodyModifier.bUpdateKinematicFromSimulation);

		if (BodyModifier.bResetToCachedTarget)
		{
			BodyModifier.bResetToCachedTarget = false;
			Implementation->ResetToCachedTarget(BodyModifier);
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
void UPhysicsControlComponent::TickComponent(
	float                        DeltaTime,
	enum ELevelTick              TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// We only want to continue the update if this is a "real" tick that corresponds to updating the
	// world. We certainly don't want to tick during a pause, because part of the processing involves 
	// (optionally) calculating target velocities based on target positions in previous ticks etc.
	if (TickType != LEVELTICK_All)
	{
		return;
	}

	UpdateTargetCaches(DeltaTime);

	UpdateControls(DeltaTime);
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
			const FName ParentBoneName = UE::PhysicsControlComponent::GetPhysicalParentBone(
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
FName UPhysicsControlComponent::CreateControl(
	UMeshComponent*               ParentMeshComponent,
	const FName                   ParentBoneName,
	UMeshComponent*               ChildMeshComponent,
	const FName                   ChildBoneName,
	const FPhysicsControlData     ControlData, 
	const FPhysicsControlTarget   ControlTarget, 
	const FPhysicsControlSettings ControlSettings,
	const FName                   Set,
	const bool                    bEnabled)
{
	const FName Name = Implementation->GetUniqueControlName(ParentBoneName, ChildBoneName);
	if (CreateNamedControl(
		Name, ParentMeshComponent, ParentBoneName, ChildMeshComponent, ChildBoneName, 
		ControlData, ControlTarget, ControlSettings, Set, bEnabled))
	{
		return Name;
	}
	return FName();
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::CreateNamedControl(
	const FName                   Name, 
	UMeshComponent*               ParentMeshComponent,
	const FName                   ParentBoneName,
	UMeshComponent*               ChildMeshComponent,
	const FName                   ChildBoneName,
	const FPhysicsControlData     ControlData, 
	const FPhysicsControlTarget   ControlTarget, 
	const FPhysicsControlSettings ControlSettings,
	const FName                   Set,
	const bool                    bEnabled)
{
	if (Implementation->FindControlRecord(Name))
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

	Implementation->NameRecords.AddControl(Name, Set);

	return true;
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
TArray<FName> UPhysicsControlComponent::CreateControlsFromSkeletalMeshBelow(
	USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   BoneName,
	const bool                    bIncludeSelf,
	const EPhysicsControlType     ControlType,
	const FPhysicsControlData     ControlData,
	const FPhysicsControlSettings ControlSettings,
	const FName                   Set,
	const bool                    bEnabled)
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
			ControlType, &ControlData, &ControlSettings, Set, &Result, bEnabled
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
				const FName ControlName = CreateControl(
					ParentMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
					ControlData, FPhysicsControlTarget(), ControlSettings, 
					FName(GetControlTypeName(ControlType).ToString().Append("_").Append(Set.ToString())), 
					bEnabled);
				if (!ControlName.IsNone())
				{
					Result.Add(ControlName);
					Implementation->NameRecords.AddControl(ControlName, GetControlTypeName(ControlType));
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
TArray<FName> UPhysicsControlComponent::CreateControlsFromSkeletalMeshAndConstraintProfileBelow(
	USkeletalMeshComponent* SkeletalMeshComponent,
	const FName             BoneName,
	const bool              bIncludeSelf,
	const FName             ConstraintProfile,
	const FName             Set,
	const bool              bEnabled)
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
			ConstraintProfile, Set, &ControlSettings, &Result, bEnabled
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
					UE_LOG(LogPhysicsControlComponent, Warning, 
						TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
					return;
				}

				UE::PhysicsControlComponent::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);

				const FName ControlName = CreateControl(
					SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
					ControlData, FPhysicsControlTarget(), ControlSettings, 
					FName(GetControlTypeName(EPhysicsControlType::ParentSpace).ToString().Append("_").Append(Set.ToString())),
					bEnabled);
				if (!ControlName.IsNone())
				{
					Result.Add(ControlName);
					Implementation->NameRecords.AddControl(
						ControlName, GetControlTypeName(EPhysicsControlType::ParentSpace));
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
TArray<FName> UPhysicsControlComponent::CreateControlsFromSkeletalMesh(
	USkeletalMeshComponent*       SkeletalMeshComponent,
	const TArray<FName>&          BoneNames,
	const EPhysicsControlType     ControlType,
	const FPhysicsControlData     ControlData,
	const FPhysicsControlSettings ControlSettings,
	const FName                   Set,
	const bool                    bEnabled)
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
		const FName ControlName = CreateControl(
			ParentMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
			ControlData, FPhysicsControlTarget(), ControlSettings, 
			FName(GetControlTypeName(ControlType).ToString().Append("_").Append(Set.ToString())),
			bEnabled);
		if (!ControlName.IsNone())
		{
			Result.Add(ControlName);
			Implementation->NameRecords.AddControl(ControlName, GetControlTypeName(ControlType));
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
TArray<FName> UPhysicsControlComponent::CreateControlsFromSkeletalMeshAndConstraintProfile(
	USkeletalMeshComponent* SkeletalMeshComponent,
	const TArray<FName>&    BoneNames,
	const FName             ConstraintProfile,
	const FName             Set,
	const bool              bEnabled)
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

		const FName ParentBoneName = 
			UE::PhysicsControlComponent::GetPhysicalParentBone(SkeletalMeshComponent, ChildBoneName);
		if (ParentBoneName.IsNone())
		{
			continue;
		}

		FPhysicsControlData ControlData;
		FConstraintProfileProperties ProfileProperties;
		if (!SkeletalMeshComponent->GetConstraintProfilePropertiesOrDefault(
			ProfileProperties, ChildBoneName, ConstraintProfile))
		{
			UE_LOG(LogPhysicsControlComponent, Warning, 
				TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
			continue;
		}

		UE::PhysicsControlComponent::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);

		const FName ControlName = CreateControl(
			SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
			ControlData, FPhysicsControlTarget(), ControlSettings, 
			FName(GetControlTypeName(EPhysicsControlType::ParentSpace).ToString().Append("_").Append(Set.ToString())),
			bEnabled);
		if (!ControlName.IsNone())
		{
			Result.Add(ControlName);
			Implementation->NameRecords.AddControl(ControlName, GetControlTypeName(EPhysicsControlType::ParentSpace));
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
TMap<FName, FPhysicsControlNames> UPhysicsControlComponent::CreateControlsFromLimbBones(
	FPhysicsControlNames&                        AllControls,
	const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
	const EPhysicsControlType                    ControlType,
	const FPhysicsControlData                    ControlData,
	const FPhysicsControlSettings                ControlSettings,
	const bool                                   bEnabled)
{
	TMap<FName, FPhysicsControlNames> Result;
	Result.Reserve(LimbBones.Num());

	for (const TPair<FName, FPhysicsControlLimbBones>& LimbBoneEntry : LimbBones)
	{
		const FName LimbName = LimbBoneEntry.Key;
		const FPhysicsControlLimbBones& BonesInLimb = LimbBoneEntry.Value;

		if (!BonesInLimb.SkeletalMeshComponent)
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No Skeletal mesh in limb %s"), *LimbName.ToString());
			continue;
		}

		USkeletalMeshComponent* ParentMeshComponent =
			(ControlType == EPhysicsControlType::ParentSpace) ? BonesInLimb.SkeletalMeshComponent : nullptr;

		const int32 NumBonesInLimb = BonesInLimb.BoneNames.Num();

		FPhysicsControlNames& LimbResult = Result.Add(LimbName);
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
			const FName ControlName = CreateControl(
				ParentMeshComponent, ParentBoneName, BonesInLimb.SkeletalMeshComponent, ChildBoneName,
				ControlData, FPhysicsControlTarget(), ControlSettings, 
				FName(GetControlTypeName(ControlType).ToString().Append("_").Append(LimbName.ToString())),
				bEnabled);
			if (!ControlName.IsNone())
			{
				LimbResult.Names.Add(ControlName);
				AllControls.Names.Add(ControlName);
				Implementation->NameRecords.AddControl(ControlName, GetControlTypeName(ControlType));
			}
			else
			{
				UE_LOG(LogPhysicsControlComponent, Warning, 
					TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
			}
		}
	}
	return Result;
}

//======================================================================================================================
TMap<FName, FPhysicsControlNames> UPhysicsControlComponent::CreateControlsFromLimbBonesAndConstraintProfile(
	FPhysicsControlNames&                        AllControls,
	const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
	const FName                                  ConstraintProfile,
	const bool                                   bEnabled)
{
	TMap<FName, FPhysicsControlNames> Result;
	Result.Reserve(LimbBones.Num());
	for (const TPair< FName, FPhysicsControlLimbBones>& LimbBoneEntry : LimbBones)
	{
		const FName LimbName = LimbBoneEntry.Key;
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

		FPhysicsControlNames& LimbResult = Result.Add(LimbName);
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

			const FName ChildBoneName = BonesInLimb.BoneNames[BoneIndex];
			FBodyInstance* ChildBone = SkeletalMeshComponent->GetBodyInstance(ChildBoneName);
			const FName ParentBoneName = 
				UE::PhysicsControlComponent::GetPhysicalParentBone(SkeletalMeshComponent, ChildBoneName);
			if (ParentBoneName.IsNone())
			{
				continue;
			}

			FPhysicsControlData ControlData;
			FConstraintProfileProperties ProfileProperties;
			if (!SkeletalMeshComponent->GetConstraintProfilePropertiesOrDefault(
				ProfileProperties, ChildBoneName, ConstraintProfile))
			
			{
				UE_LOG(LogPhysicsControlComponent, Warning, 
					TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
				continue;
			}

			UE::PhysicsControlComponent::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);

			const FName ControlName = CreateControl(
				SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
				ControlData, FPhysicsControlTarget(), ControlSettings, 
				FName(GetControlTypeName(EPhysicsControlType::ParentSpace).ToString().Append("_").Append(LimbName.ToString())),
				bEnabled);
			if (!ControlName.IsNone())
			{
				LimbResult.Names.Add(ControlName);
				AllControls.Names.Add(ControlName);
				Implementation->NameRecords.AddControl(ControlName, GetControlTypeName(EPhysicsControlType::ParentSpace));
			}
			else
			{
				UE_LOG(LogPhysicsControlComponent, Warning, 
					TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
			}
		}
	}
	return Result;
}

//======================================================================================================================
bool UPhysicsControlComponent::DestroyControl(const FName Name)
{
	return Implementation->DestroyControl(Name);
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyControls(const TArray<FName>& Names)
{
	for (FName Name : Names)
	{
		Implementation->DestroyControl(Name);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyControlsInSet(const FName SetName)
{
	// Make a copy as the set will be being modified during 
	TArray<FName> Names = GetControlNamesInSet(SetName);
	DestroyControls(Names);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlEnabled(const FName Name, const bool bEnable)
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
void UPhysicsControlComponent::SetControlsEnabled(const TArray<FName>& Names, const bool bEnable)
{
	for (FName Name : Names)
	{
		SetControlEnabled(Name, bEnable);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlsInSetEnabled(FName SetName, bool bEnable)
{
	SetControlsEnabled(GetControlNamesInSet(SetName), bEnable);
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetControlData(
	const FName               Name, 
	const FPhysicsControlData ControlData, 
	const bool                bEnableControl)
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
void UPhysicsControlComponent::SetControlDatas(
	const TArray<FName>&       Names, 
	const FPhysicsControlData  ControlData, 
	const bool                 bEnableControl)
{
	for (FName Name : Names)
	{
		SetControlData(Name, ControlData, bEnableControl);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlDatasInSet(
	const FName                SetName,
	const FPhysicsControlData  ControlData,
	const bool                 bEnableControl)
{
	SetControlDatas(GetControlNamesInSet(SetName), ControlData, bEnableControl);
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetControlMultiplier(
	const FName                      Name, 
	const FPhysicsControlMultiplier  ControlMultiplier, 
	const bool                       bEnableControl)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlMultiplier = ControlMultiplier;
		if (bEnableControl)
		{
			Record->PhysicsControlState.bEnabled = true;
		}
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlMultipliers(
	const TArray<FName>&             Names,
	const FPhysicsControlMultiplier  ControlMultiplier, 
	const bool                       bEnableControl)
{
	for (FName Name : Names)
	{
		SetControlMultiplier(Name, ControlMultiplier, bEnableControl);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlMultipliersInSet(
	const FName                      SetName,
	const FPhysicsControlMultiplier  ControlMultiplier, 
	const bool                       bEnableControl)
{
	SetControlMultipliers(GetControlNamesInSet(SetName), ControlMultiplier, bEnableControl);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlLinearData(
	const FName Name, 
	const float Strength, 
	const float DampingRatio, 
	const float ExtraDamping, 
	const float MaxForce, 
	const bool  bEnableControl)
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
	const FName Name, 
	const float Strength, 
	const float DampingRatio, 
	const float ExtraDamping, 
	const float MaxTorque, 
	const bool  bEnableControl)
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
	const FName                 Name, 
	const FPhysicsControlTarget ControlTarget, 
	const bool                  bEnableControl)
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
void UPhysicsControlComponent::SetControlTargets(
	const TArray<FName>&        Names, 
	const FPhysicsControlTarget ControlTarget, 
	const bool                  bEnableControl)
{
	for (FName Name : Names)
	{
		SetControlTarget(Name, ControlTarget, bEnableControl);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlTargetsInSet(
	const FName                 SetName, 
	const FPhysicsControlTarget ControlTarget, 
	const bool                  bEnableControl)
{
	SetControlTargets(GetControlNamesInSet(SetName), ControlTarget, bEnableControl);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetPositionAndOrientation(
	const FName    Name, 
	const FVector  Position, 
	const FRotator Orientation, 
	const float    VelocityDeltaTime, 
	const bool     bEnableControl, 
	const bool     bApplyControlPointToTarget)
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
void UPhysicsControlComponent::SetControlTargetPositionsAndOrientations(
	const TArray<FName>& Names,
	const FVector        Position,
	const FRotator       Orientation,
	const float          VelocityDeltaTime,
	const bool           bEnableControl,
	const bool           bApplyControlPointToTarget)
{
	for (FName Name : Names)
	{
		SetControlTargetPositionAndOrientation(
			Name, Position, Orientation, VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlTargetPositionsAndOrientationsInSet(
	const FName          SetName,
	const FVector        Position,
	const FRotator       Orientation,
	const float          VelocityDeltaTime,
	const bool           bEnableControl,
	const bool           bApplyControlPointToTarget)
{
	SetControlTargetPositionsAndOrientations(GetControlNamesInSet(SetName), 
		Position, Orientation, VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetPosition(
	const FName   Name, 
	const FVector Position, 
	const float   VelocityDeltaTime, 
	const bool    bEnableControl, 
	const bool    bApplyControlPointToTarget)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		if (VelocityDeltaTime != 0)
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
void UPhysicsControlComponent::SetControlTargetPositions(
	const TArray<FName>& Names,
	const FVector        Position,
	const float          VelocityDeltaTime,
	const bool           bEnableControl,
	const bool           bApplyControlPointToTarget)
{
	for (FName Name : Names)
	{
		SetControlTargetPosition(
			Name, Position, VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlTargetPositionsInSet(
	const FName   SetName,
	const FVector Position,
	const float   VelocityDeltaTime,
	const bool    bEnableControl,
	const bool    bApplyControlPointToTarget)
{
	SetControlTargetPositions(GetControlNamesInSet(SetName),
		Position, VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetOrientation(
	const FName    Name, 
	const FRotator Orientation, 
	const float    AngularVelocityDeltaTime, 
	const bool     bEnableControl, 
	const bool     bApplyControlPointToTarget)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		if (AngularVelocityDeltaTime != 0)
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
void UPhysicsControlComponent::SetControlTargetOrientations(
	const TArray<FName>& Names,
	const FRotator Orientation,
	const float    AngularVelocityDeltaTime,
	const bool     bEnableControl,
	const bool     bApplyControlPointToTarget)
{
	for (FName Name : Names)
	{
		SetControlTargetOrientation(
			Name, Orientation, AngularVelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlTargetOrientationsInSet(
	const FName    SetName,
	const FRotator Orientation,
	const float    AngularVelocityDeltaTime,
	const bool     bEnableControl,
	const bool     bApplyControlPointToTarget)
{
	SetControlTargetOrientations(GetControlNamesInSet(SetName),
		Orientation, AngularVelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetPositionsFromArray(
	const TArray<FName>&   Names,
	const TArray<FVector>& Positions,
	const float            VelocityDeltaTime,
	const bool             bEnableControl,
	const bool             bApplyControlPointToTarget)
{
	int32 NumControlNames = Names.Num();
	int32 NumPositions = Positions.Num();
	if (NumControlNames != NumPositions)
	{
		return false;
	}
	for (int32 Index = 0 ; Index != NumControlNames ; ++Index)
	{
		SetControlTargetPosition(
			Names[Index], Positions[Index], VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
	}
	return true;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetOrientationsFromArray(
	const TArray<FName>&    Names,
	const TArray<FRotator>& Orientations,
	const float             VelocityDeltaTime,
	const bool              bEnableControl,
	const bool              bApplyControlPointToTarget)
{
	int32 NumControlNames = Names.Num();
	int32 NumOrientations = Orientations.Num();
	if (NumControlNames != NumOrientations)
	{
		return false;
	}
	for (int32 Index = 0 ; Index != NumControlNames ; ++Index)
	{
		SetControlTargetOrientation(
			Names[Index], Orientations[Index], VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
	}
	return true;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetPositionsAndOrientationsFromArray(
	const TArray<FName>&    Names,
	const TArray<FVector>&  Positions,
	const TArray<FRotator>& Orientations,
	const float             VelocityDeltaTime,
	const bool              bEnableControl,
	const bool              bApplyControlPointToTarget)
{
	int32 NumControlNames = Names.Num();
	int32 NumPositions = Positions.Num();
	int32 NumOrientations = Orientations.Num();
	if (NumControlNames != NumPositions || NumControlNames != NumOrientations)
	{
		return false;
	}
	for (int32 Index = 0; Index != NumControlNames; ++Index)
	{
		SetControlTargetPositionAndOrientation(
			Names[Index], Positions[Index], Orientations[Index], VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
	}
	return true;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTargetPoses(
	const FName    Name,
	const FVector  ParentPosition, 
	const FRotator ParentOrientation,
	const FVector  ChildPosition, 
	const FRotator ChildOrientation,
	const float    VelocityDeltaTime, 
	const bool     bEnableControl)
{
	FPhysicsControlRecord* Record = Implementation->FindControlRecord(Name);
	if (Record)
	{
		FTransform ParentTM(ParentOrientation, ParentPosition, FVector::One());
		FTransform ChildTM(ChildOrientation, ChildPosition, FVector::One());

		FTransform OffsetTM = ChildTM * ParentTM.Inverse();
		FVector Position = OffsetTM.GetTranslation();
		FQuat OrientationQ = OffsetTM.GetRotation();

		if (VelocityDeltaTime != 0)
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
	const bool  bUseSkeletalAnimation,
	const float SkeletalAnimationVelocityMultiplier)
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
void UPhysicsControlComponent::SetControlsUseSkeletalAnimation(
	const TArray<FName>& Names,
	const bool           bUseSkeletalAnimation,
	const float          SkeletalAnimationVelocityMultiplier)
{
	for (FName Name : Names)
	{
		SetControlUseSkeletalAnimation(Name, bUseSkeletalAnimation, SkeletalAnimationVelocityMultiplier);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlsInSetUseSkeletalAnimation(
	const FName SetName,
	const bool  bUseSkeletalAnimation,
	const float SkeletalAnimationVelocityMultiplier)
{
	SetControlsUseSkeletalAnimation(
		GetControlNamesInSet(SetName), bUseSkeletalAnimation, SkeletalAnimationVelocityMultiplier);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlAutoDisable(const FName Name, const bool bAutoDisable)
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
void UPhysicsControlComponent::SetControlsAutoDisable(const TArray<FName>& Names, const bool bAutoDisable)
{
	for (FName Name : Names)
	{
		SetControlAutoDisable(Name, bAutoDisable);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlsInSetAutoDisable(const FName SetName, const bool bAutoDisable)
{
	SetControlsAutoDisable(GetControlNamesInSet(SetName), bAutoDisable);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlDisableCollision(const FName Name, const bool bDisableCollision)
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		PhysicsControl->ControlSettings.bDisableCollision = bDisableCollision;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlsDisableCollision(const TArray<FName>& Names, const bool bDisableCollision)
{
	for (FName Name : Names)
	{
		SetControlDisableCollision(Name, bDisableCollision);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlsInSetDisableCollision(const FName SetName, const bool bDisableCollision)
{
	SetControlsDisableCollision(GetControlNamesInSet(SetName), bDisableCollision);
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
bool UPhysicsControlComponent::GetControlMultiplier(const FName Name, FPhysicsControlMultiplier& ControlMultiplier) const
{
	FPhysicsControl* PhysicsControl = Implementation->FindControl(Name);
	if (PhysicsControl)
	{
		ControlMultiplier = PhysicsControl->ControlMultiplier;
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
FName UPhysicsControlComponent::CreateBodyModifier(
	UMeshComponent*               MeshComponent,
	const FName                   BoneName,
	const FName                   Set,
	const EPhysicsMovementType    MovementType,
	const ECollisionEnabled::Type CollisionType,
	const float                   GravityMultiplier,
	const float                   PhysicsBlendWeight,
	const bool                    bUseSkeletalAnimation,
	const bool                    bUpdateKinematicFromSimulation)
{
	const FName Name = Implementation->GetUniqueBodyModifierName(BoneName);
	if (CreateNamedBodyModifier(
		Name, MeshComponent, BoneName, Set, MovementType, CollisionType, 
		GravityMultiplier, PhysicsBlendWeight, bUseSkeletalAnimation, bUpdateKinematicFromSimulation))
	{
		return Name;
	}
	return FName();
}

//======================================================================================================================
bool UPhysicsControlComponent::CreateNamedBodyModifier(
	const FName                   Name,
	UMeshComponent*               MeshComponent,
	const FName                   BoneName,
	const FName                   Set,
	const EPhysicsMovementType    MovementType,
	const ECollisionEnabled::Type CollisionType,
	const float                   GravityMultiplier,
	const float                   PhysicsBlendWeight,
	const bool                    bUseSkeletalAnimation,
	const bool                    bUpdateKinematicFromSimulation)
{
	if (Implementation->FindBodyModifier(Name))
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
			MeshComponent, BoneName, MovementType, CollisionType, 
			GravityMultiplier, PhysicsBlendWeight, bUseSkeletalAnimation, bUpdateKinematicFromSimulation));

	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent);
	if (SkeletalMeshComponent)
	{
		Implementation->AddSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		Implementation->AddSkeletalMeshReferenceForModifier(SkeletalMeshComponent);
	}

	Implementation->NameRecords.AddBodyModifier(Name, Set);

	return true;
}

//======================================================================================================================
TArray<FName> UPhysicsControlComponent::CreateBodyModifiersFromSkeletalMeshBelow(
	USkeletalMeshComponent*       SkeletalMeshComponent,
	const FName                   BoneName,
	const bool                    bIncludeSelf,
	const FName                   Set,
	const EPhysicsMovementType    MovementType,
	const ECollisionEnabled::Type CollisionType,
	const float                   GravityMultiplier,
	const float                   PhysicsBlendWeight,
	const bool                    bUseSkeletalAnimation,
	const bool                    bUpdateKinematicFromSimulation)
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
			this, PhysicsAsset, SkeletalMeshComponent, Set, MovementType, CollisionType,
			GravityMultiplier, PhysicsBlendWeight, bUseSkeletalAnimation, bUpdateKinematicFromSimulation, &Result
		](const FBodyInstance* BI)
		{
			if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
			{
				const FName BoneName = PhysicsAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;
				const FName BodyModifierName = CreateBodyModifier(
					SkeletalMeshComponent, BoneName, Set, MovementType, CollisionType, 
					GravityMultiplier, PhysicsBlendWeight, bUseSkeletalAnimation, bUpdateKinematicFromSimulation);
				Result.Add(BodyModifierName);
			}
		});

	return Result;
}

//======================================================================================================================
TMap<FName, FPhysicsControlNames> UPhysicsControlComponent::CreateBodyModifiersFromLimbBones(
	FPhysicsControlNames&                        AllBodyModifiers,
	const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
	const EPhysicsMovementType                   MovementType,
	const ECollisionEnabled::Type                CollisionType,
	const float                                  GravityMultiplier,
	const float                                  PhysicsBlendWeight,
	const bool                                   bUseSkeletalAnimation,
	const bool                                   bUpdateKinematicFromSimulation)
{
	TMap<FName, FPhysicsControlNames> Result;
	Result.Reserve(LimbBones.Num());

	for (const TPair<FName, FPhysicsControlLimbBones>& LimbBoneEntry : LimbBones)
	{
		const FName LimbName = LimbBoneEntry.Key;
		const FPhysicsControlLimbBones& BonesInLimb = LimbBoneEntry.Value;

		if (!BonesInLimb.SkeletalMeshComponent)
		{
			UE_LOG(LogPhysicsControlComponent, Warning, TEXT("No Skeletal mesh in limb %s"), *LimbName.ToString());
			continue;
		}

		const int32 NumBonesInLimb = BonesInLimb.BoneNames.Num();

		FPhysicsControlNames& LimbResult = Result.Add(LimbName);
		LimbResult.Names.Reserve(NumBonesInLimb);
		AllBodyModifiers.Names.Reserve(AllBodyModifiers.Names.Num() + NumBonesInLimb);

		for (const FName BoneName : BonesInLimb.BoneNames)
		{
			const FName BodyModifierName = CreateBodyModifier(
				BonesInLimb.SkeletalMeshComponent, BoneName, LimbName, MovementType, CollisionType, 
				GravityMultiplier, PhysicsBlendWeight, bUseSkeletalAnimation, bUpdateKinematicFromSimulation);
			if (!BodyModifierName.IsNone())
			{
				LimbResult.Names.Add(BodyModifierName);
				AllBodyModifiers.Names.Add(BodyModifierName);
			}
			else
			{
				UE_LOG(LogPhysicsControlComponent, Warning, 
					TEXT("Failed to make body modifier for %s"), *BoneName.ToString());
			}
		}
	}
	return Result;
}

//======================================================================================================================
bool UPhysicsControlComponent::DestroyBodyModifier(const FName Name)
{
	return Implementation->DestroyBodyModifier(Name);
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyBodyModifiers(const TArray<FName>& Names)
{
	for (FName Name : Names)
	{
		Implementation->DestroyBodyModifier(Name);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyBodyModifiersInSet(const FName SetName)
{
	TArray<FName> Names = GetBodyModifierNamesInSet(SetName);
	DestroyBodyModifiers(Names);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierKinematicTarget(
	const FName    Name,
	const FVector  KinematicTargetPosition,
	const FRotator KinematicTargetOrienation,
	const bool     bMakeKinematic)
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
	const FName                Name,
	const EPhysicsMovementType MovementType)
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
void UPhysicsControlComponent::SetBodyModifiersMovementType(
	const TArray<FName>&       Names,
	const EPhysicsMovementType MovementType)
{
	for (FName Name : Names)
	{
		SetBodyModifierMovementType(Name, MovementType);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifiersInSetMovementType(
	const FName                SetName,
	const EPhysicsMovementType MovementType)
{
	SetBodyModifiersMovementType(GetBodyModifierNamesInSet(SetName), MovementType);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierCollisionType(
	const FName                   Name,
	const ECollisionEnabled::Type CollisionType)
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
void UPhysicsControlComponent::SetBodyModifiersCollisionType(
	const TArray<FName>&          Names,
	const ECollisionEnabled::Type CollisionType)
{
	for (FName Name : Names)
	{
		SetBodyModifierCollisionType(Name, CollisionType);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifiersInSetCollisionType(
	const FName                   SetName,
	const ECollisionEnabled::Type CollisionType)
{
	SetBodyModifiersCollisionType(GetBodyModifierNamesInSet(SetName), CollisionType);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierGravityMultiplier(
	const FName Name,
	const float GravityMultiplier)
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
void UPhysicsControlComponent::SetBodyModifiersGravityMultiplier(
	const TArray<FName>& Names,
	const float          GravityMultiplier)
{
	for (FName Name : Names)
	{
		SetBodyModifierGravityMultiplier(Name, GravityMultiplier);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifiersInSetGravityMultiplier(
	const FName SetName,
	const float GravityMultiplier)
{
	SetBodyModifiersGravityMultiplier(GetBodyModifierNamesInSet(SetName), GravityMultiplier);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierPhysicsBlendWeight(
	const FName Name,
	const float PhysicsBlendWeight)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		PhysicsBodyModifier->PhysicsBlendWeight = PhysicsBlendWeight;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifiersPhysicsBlendWeight(
	const TArray<FName>& Names,
	const float          PhysicsBlendWeight)
{
	for (FName Name : Names)
	{
		SetBodyModifierPhysicsBlendWeight(Name, PhysicsBlendWeight);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifiersInSetPhysicsBlendWeight(
	const FName SetName,
	const float PhysicsBlendWeight)
{
	SetBodyModifiersPhysicsBlendWeight(GetBodyModifierNamesInSet(SetName), PhysicsBlendWeight);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierUseSkeletalAnimation(
	const FName Name,
	const bool  bUseSkeletalAnimation)
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
void UPhysicsControlComponent::SetBodyModifiersUseSkeletalAnimation(
	const TArray<FName>& Names,
	const bool           bUseSkeletalAnimation)
{
	for (FName Name : Names)
	{
		SetBodyModifierUseSkeletalAnimation(Name, bUseSkeletalAnimation);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifiersInSetUseSkeletalAnimation(
	const FName SetName,
	const bool  bUseSkeletalAnimation)
{
	SetBodyModifiersUseSkeletalAnimation(GetBodyModifierNamesInSet(SetName), bUseSkeletalAnimation);
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierUpdateKinematicFromSimulation(
	const FName Name,
	const bool  bUpdateKinematicFromSimulation)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		PhysicsBodyModifier->bUpdateKinematicFromSimulation = bUpdateKinematicFromSimulation;
		return true;
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifiersUpdateKinematicFromSimulation(
	const TArray<FName>& Names,
	const bool           bUpdateKinematicFromSimulation)
{
	for (FName Name : Names)
	{
		SetBodyModifierUpdateKinematicFromSimulation(Name, bUpdateKinematicFromSimulation);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifiersInSetUpdateKinematicFromSimulation(
	const FName SetName,
	const bool  bUpdateKinematicFromSimulation)
{
	SetBodyModifiersUpdateKinematicFromSimulation(GetBodyModifierNamesInSet(SetName), bUpdateKinematicFromSimulation);
}

//======================================================================================================================
const TArray<FName>& UPhysicsControlComponent::GetAllControlNames() const
{
	return GetControlNamesInSet("All");
}

//======================================================================================================================
void UPhysicsControlComponent::CreateControlsAndBodyModifiersFromLimbBones(
	FPhysicsControlNames&                       AllWorldSpaceControls,
	TMap<FName, FPhysicsControlNames>&          LimbWorldSpaceControls,
	FPhysicsControlNames&                       AllParentSpaceControls,
	TMap<FName, FPhysicsControlNames>&          LimbParentSpaceControls,
	FPhysicsControlNames&                       AllBodyModifiers,
	TMap<FName, FPhysicsControlNames>&          LimbBodyModifiers,
	USkeletalMeshComponent*                     SkeletalMeshComponent,
	const TArray<FPhysicsControlLimbSetupData>& LimbSetupData,
	const FPhysicsControlData                   WorldSpaceControlData,
	const FPhysicsControlSettings               WorldSpaceControlSettings,
	const bool                                  bEnableWorldSpaceControls,
	const FPhysicsControlData                   ParentSpaceControlData,
	const FPhysicsControlSettings               ParentSpaceControlSettings,
	const bool                                  bEnableParentSpaceControls,
	const EPhysicsMovementType                  PhysicsMovementType,
	const float                                 GravityMultiplier,
	const float                                 PhysicsBlendWeight)
{
	TMap<FName, FPhysicsControlLimbBones> LimbBones = 
		GetLimbBonesFromSkeletalMesh(SkeletalMeshComponent, LimbSetupData);

	LimbWorldSpaceControls = CreateControlsFromLimbBones(
		AllWorldSpaceControls, LimbBones, EPhysicsControlType::WorldSpace, 
		WorldSpaceControlData, WorldSpaceControlSettings, bEnableWorldSpaceControls);

	LimbParentSpaceControls = CreateControlsFromLimbBones(
		AllParentSpaceControls, LimbBones, EPhysicsControlType::ParentSpace,
		ParentSpaceControlData, ParentSpaceControlSettings, bEnableParentSpaceControls);

	LimbBodyModifiers = CreateBodyModifiersFromLimbBones(
		AllBodyModifiers, LimbBones, PhysicsMovementType, ECollisionEnabled::QueryAndPhysics, 
		GravityMultiplier, PhysicsBlendWeight, true);
}

//======================================================================================================================
void UPhysicsControlComponent::AddControlToSet(
	FPhysicsControlNames& NewSet, 
	const FName           Control, 
	const FName           SetName)
{
	Implementation->NameRecords.AddControl(Control, SetName);
	NewSet.Names = GetControlNamesInSet(SetName);
}

//======================================================================================================================
void UPhysicsControlComponent::AddControlsToSet(
	FPhysicsControlNames& NewSet, 
	const TArray<FName>&  Controls, 
	const FName           SetName)
{
	for (FName Control : Controls)
	{
		Implementation->NameRecords.AddControl(Control, SetName);
	}
	NewSet.Names = GetControlNamesInSet(SetName);
}

//======================================================================================================================
const TArray<FName>& UPhysicsControlComponent::GetControlNamesInSet(const FName SetName) const
{
	return Implementation->NameRecords.GetControlNamesInSet(SetName);
}

//======================================================================================================================
const TArray<FName>& UPhysicsControlComponent::GetAllBodyModifierNames() const
{
	return GetBodyModifierNamesInSet("All");
}

//======================================================================================================================
const TArray<FName>& UPhysicsControlComponent::GetBodyModifierNamesInSet(const FName SetName) const
{
	return Implementation->NameRecords.GetBodyModifierNamesInSet(SetName);
}

//======================================================================================================================
void UPhysicsControlComponent::AddBodyModifierToSet(
	FPhysicsControlNames& NewSet, 
	const FName           BodyModifier, 
	const FName           SetName)
{
	Implementation->NameRecords.AddBodyModifier(BodyModifier, SetName);
	NewSet.Names = GetBodyModifierNamesInSet(SetName);
}


//======================================================================================================================
void UPhysicsControlComponent::AddBodyModifiersToSet(
	FPhysicsControlNames& NewSet, 
	const TArray<FName>&  BodyModifiers, 
	const FName           SetName)
{
	for (FName BodyModifier : BodyModifiers)
	{
		Implementation->NameRecords.AddBodyModifier(BodyModifier, SetName);
	}
	NewSet.Names = GetBodyModifierNamesInSet(SetName);
}

//======================================================================================================================
TArray<FName> UPhysicsControlComponent::GetSetsContainingControl(const FName Control) const
{
	TArray<FName> Result;
	for (const TPair<FName, TArray<FName>>& ControlSetPair : Implementation->NameRecords.ControlSets)
	{
		for (const FName& ControlName : ControlSetPair.Value)
		{
			if (ControlName == Control)
			{
				Result.Add(ControlSetPair.Key);
			}
		}
	}
	return Result;
}

//======================================================================================================================
TArray<FName> UPhysicsControlComponent::GetSetsContainingBodyModifier(const FName BodyModifier) const
{
	TArray<FName> Result;
	for (const TPair<FName, TArray<FName>>& BodyModifierSetPair : Implementation->NameRecords.BodyModifierSets)
	{
		for (const FName& BodyModifierName : BodyModifierSetPair.Value)
		{
			if (BodyModifierName == BodyModifier)
			{
				Result.Add(BodyModifierSetPair.Key);
			}
		}
	}
	return Result;
}

//======================================================================================================================
TArray<FTransform> UPhysicsControlComponent::GetCachedBoneTransforms(
	const USkeletalMeshComponent* SkeletalMeshComponent, 
	const TArray<FName>&          BoneNames)
{
	TArray<FTransform> Result;
	Result.Reserve(BoneNames.Num());
	FCachedSkeletalMeshData::FBoneData BoneData;

	for (const FName& BoneName : BoneNames)
	{
		if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
		{
			FTransform BoneTransform(BoneData.Orientation, BoneData.Position);
			Result.Add(BoneTransform);
		}
		else
		{
			Result.Add(FTransform::Identity);
		}
	}
	return Result;
}

//======================================================================================================================
TArray<FVector> UPhysicsControlComponent::GetCachedBonePositions(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const TArray<FName>&          BoneNames)
{
	TArray<FVector> Result;
	Result.Reserve(BoneNames.Num());
	FCachedSkeletalMeshData::FBoneData BoneData;

	for (const FName& BoneName : BoneNames)
	{
		if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
		{
			Result.Add(BoneData.Position);
		}
		else
		{
			Result.Add(FVector::ZeroVector);
		}
	}
	return Result;
}

//======================================================================================================================
TArray<FRotator> UPhysicsControlComponent::GetCachedBoneOrientations(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const TArray<FName>&          BoneNames)
{
	TArray<FRotator> Result;
	Result.Reserve(BoneNames.Num());
	FCachedSkeletalMeshData::FBoneData BoneData;

	for (const FName& BoneName : BoneNames)
	{
		if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
		{
			Result.Add(BoneData.Orientation.Rotator());
		}
		else
		{
			Result.Add(FRotator::ZeroRotator);
		}
	}
	return Result;
}

//======================================================================================================================
TArray<FVector> UPhysicsControlComponent::GetCachedBoneVelocities(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const TArray<FName>&          BoneNames)
{
	TArray<FVector> Result;
	Result.Reserve(BoneNames.Num());
	FCachedSkeletalMeshData::FBoneData BoneData;

	for (const FName& BoneName : BoneNames)
	{
		if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
		{
			Result.Add(BoneData.Velocity);
		}
		else
		{
			Result.Add(FVector::ZeroVector);
		}
	}

	return Result;
}

//======================================================================================================================
TArray<FVector> UPhysicsControlComponent::GetCachedBoneAngularVelocities(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const TArray<FName>&          BoneNames)
{
	TArray<FVector> Result;
	Result.Reserve(BoneNames.Num());
	FCachedSkeletalMeshData::FBoneData BoneData;

	for (const FName& BoneName : BoneNames)
	{
		if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
		{
			Result.Add(BoneData.AngularVelocity);
		}
		else
		{
			Result.Add(FVector::ZeroVector);
		}
	}

	return Result;
}

//======================================================================================================================
FTransform UPhysicsControlComponent::GetCachedBoneTransform(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   BoneName)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
	{
		return FTransform(BoneData.Orientation, BoneData.Position);
	}
	return FTransform();
}

//======================================================================================================================
FVector UPhysicsControlComponent::GetCachedBonePosition(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   BoneName)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
	{
		return BoneData.Position;
	}
	return FVector::ZeroVector;
}

//======================================================================================================================
FRotator UPhysicsControlComponent::GetCachedBoneOrientation(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   BoneName)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
	{
		return BoneData.Orientation.Rotator();
	}
	return FRotator::ZeroRotator;
}

//======================================================================================================================
FVector UPhysicsControlComponent::GetCachedBoneVelocity(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   BoneName)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
	{
		return BoneData.Velocity;
	}
	return FVector::Zero();
}

//======================================================================================================================
FVector UPhysicsControlComponent::GetCachedBoneAngularVelocity(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   BoneName)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (Implementation->GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
	{
		return BoneData.AngularVelocity;
	}
	return FVector::Zero();
}

//======================================================================================================================
bool UPhysicsControlComponent::SetCachedBoneData(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   BoneName,
	const FTransform&             TM,
	const FVector                 Velocity,
	const FVector                 AngularVelocity)
{
	FCachedSkeletalMeshData::FBoneData* BoneData;
	if (Implementation->GetModifiableBoneData(BoneData, SkeletalMeshComponent, BoneName))
	{
		BoneData->Position = TM.GetLocation();
		BoneData->Orientation = TM.GetRotation();
		BoneData->Velocity = Velocity;
		BoneData->AngularVelocity = AngularVelocity;
		return true;
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::ResetBodyModifierToCachedBoneTransform(
	const FName                        Name,
	const EResetToCachedTargetBehavior Behavior)
{
	FPhysicsBodyModifier* PhysicsBodyModifier = Implementation->FindBodyModifier(Name);
	if (PhysicsBodyModifier)
	{
		if (Behavior == EResetToCachedTargetBehavior::ResetImmediately)
		{
			Implementation->ResetToCachedTarget(*PhysicsBodyModifier);
		}
		else
		{
			PhysicsBodyModifier->bResetToCachedTarget = true;
		}
		return true;
	}
	return false;

}

//======================================================================================================================
void UPhysicsControlComponent::ResetBodyModifiersToCachedBoneTransforms(
	const TArray<FName>&               Names,
	const EResetToCachedTargetBehavior Behavior)
{
	for (FName Name : Names)
	{
		ResetBodyModifierToCachedBoneTransform(Name, Behavior);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::ResetBodyModifiersInSetToCachedBoneTransforms(
	const FName                        SetName,
	const EResetToCachedTargetBehavior Behavior)
{
	ResetBodyModifiersToCachedBoneTransforms(GetBodyModifierNamesInSet(SetName), Behavior);
}


#if WITH_EDITOR

//======================================================================================================================
void UPhysicsControlComponent::OnRegister()
{
	Super::OnRegister();

	if (SpriteComponent)
	{
		UpdateSpriteTexture();
		SpriteComponent->SpriteInfo.Category = TEXT("Physics");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Physics", "Physics");
	}
}

//======================================================================================================================
void UPhysicsControlComponent::UpdateSpriteTexture()
{
	if (SpriteComponent)
	{
		SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KBSJoint.S_KBSJoint")));
	}
}

//======================================================================================================================
void UPhysicsControlComponent::DebugDraw(FPrimitiveDrawInterface* PDI) const
{
	// Draw gizmos
	if (bShowDebugVisualization && VisualizationSizeScale > 0)
	{
		for (const TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair : 
			Implementation->PhysicsControlRecords)
		{
			const FName Name = PhysicsControlRecordPair.Key;
			const FPhysicsControlRecord& Record = PhysicsControlRecordPair.Value;
			DebugDrawControl(PDI, Record, Name);
		} 
	}

	// Detailed controls - if there's a filter
	if (!DebugControlDetailFilter.IsEmpty())
	{
		for (const TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair :
			Implementation->PhysicsControlRecords)
		{
			const FName Name = PhysicsControlRecordPair.Key;

			if (Name.ToString().Contains(DebugControlDetailFilter))
			{
				const FPhysicsControlRecord& Record = PhysicsControlRecordPair.Value;

				FString ParentComponentName = Record.PhysicsControl.ParentMeshComponent ?
					Record.PhysicsControl.ParentMeshComponent->GetName() : TEXT("NoParent");
				FString ChildComponentName = Record.PhysicsControl.ChildMeshComponent ?
					Record.PhysicsControl.ChildMeshComponent->GetName() : TEXT("NoChild");

				FString Text = FString::Printf(
					TEXT("%s: Parent %s (%s) Child %s (%s): Linear strength %f Angular strength %f"),
					*Name.ToString(),
					*ParentComponentName,
					*Record.PhysicsControl.ParentBoneName.ToString(),
					*ChildComponentName,
					*Record.PhysicsControl.ChildBoneName.ToString(),
					Record.PhysicsControl.ControlData.LinearStrength,
					Record.PhysicsControl.ControlData.AngularStrength);

				GEngine->AddOnScreenDebugMessage(
					-1, 0.0f,
					Record.PhysicsControlState.bEnabled ? FColor::Green : FColor::Red, Text);
			}
		}
	}

	// Summary of control list
	if (bShowDebugControlList)
	{
		FString AllNames;

		for (const TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair :
			Implementation->PhysicsControlRecords)
		{
			const FName Name = PhysicsControlRecordPair.Key;
			AllNames += Name.ToString() + TEXT(" ");
			if (AllNames.Len() > 256)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, *AllNames);
				AllNames.Reset();
			}
		}
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::White,
			FString::Printf(TEXT("%d Controls: %s"), Implementation->PhysicsControlRecords.Num(), *AllNames));

	}

	// Detailed body modifiers - if there's a filter
	if (!DebugBodyModifierDetailFilter.IsEmpty())
	{
		for (const TPair<FName, FPhysicsBodyModifier>& PhysicsBodyModifierPair :
			Implementation->PhysicsBodyModifiers)
		{
			const FName Name = PhysicsBodyModifierPair.Key;

			if (Name.ToString().Contains(DebugBodyModifierDetailFilter))
			{
				const FPhysicsBodyModifier& Record = PhysicsBodyModifierPair.Value;

				FString ComponentName = Record.MeshComponent ? Record.MeshComponent->GetName() : TEXT("None");

				FString Text = FString::Printf(
					TEXT("%s: %s: %s %s GravityMultiplier %f BlendWeight %f"),
					*Name.ToString(),
					*ComponentName,
					*UEnum::GetValueAsString(Record.MovementType),
					*UEnum::GetValueAsString(Record.CollisionType),
					Record.GravityMultiplier,
					Record.PhysicsBlendWeight);

				GEngine->AddOnScreenDebugMessage(-1, 0.0f, 
					Record.MovementType == EPhysicsMovementType::Simulated ? FColor::Green : FColor::Red, Text);
			}
		}
	}

	// Summary of control list
	if (bShowDebugBodyModifierList)
	{
		FString AllNames;

		for (const TPair<FName, FPhysicsBodyModifier>& PhysicsBodyModifierPair : Implementation->PhysicsBodyModifiers)
		{
			const FName Name = PhysicsBodyModifierPair.Key;
			AllNames += Name.ToString() + TEXT(" ");
			if (AllNames.Len() > 256)
			{
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, *AllNames);
				AllNames.Reset();
			}
		}
		GEngine->AddOnScreenDebugMessage(
			-1, 0.0f, FColor::White,
			FString::Printf(TEXT("%d Body modifiers: %s"), Implementation->PhysicsBodyModifiers.Num(), *AllNames));

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

	const bool bHaveLinear = Record.PhysicsControl.ControlData.LinearStrength > 0;
	const bool bHaveAngular = Record.PhysicsControl.ControlData.AngularStrength > 0;

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
		const FTransform ParentBodyTM = ParentBodyInstance ? ParentBodyInstance->GetUnrealWorldTransform() : FTransform();

		FTransform TargetTM;
		FVector TargetVelocity;
		FVector TargetAngularVelocity;
		Implementation->CalculateControlTargetData(TargetTM, TargetVelocity, TargetAngularVelocity, Record, true);

		// WorldChildFrameTM is the world-space transform of the child (driven) constraint frame
		const FTransform WorldChildFrameTM = ConstraintInstance->GetRefFrame(EConstraintFrame::Frame1) * ChildBodyTM;

		// WorldParentFrameTM is the world-space transform of the parent constraint frame
		const FTransform WorldParentFrameTM = ConstraintInstance->GetRefFrame(EConstraintFrame::Frame2) * ParentBodyTM;

		const FTransform WorldCurrentTM = WorldChildFrameTM;

		FTransform WorldTargetTM = TargetTM * WorldParentFrameTM;
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
			if (VelocityPredictionTime != 0)
			{
				DrawCoordinateSystem(
					PDI, PredictedTargetTM.GetTranslation(), PredictedTargetTM.Rotator(),
					VisualizationSizeScale * 0.5f, SDPG_Foreground, 4.0f * GizmoWidthScale);
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
			if (VelocityPredictionTime != 0)
			{
				DrawWireSphere(
					PDI, PredictedTargetTM, TargetColor, 
					VisualizationSizeScale * 0.5f, 8, SDPG_Foreground, 3.0f * GizmoWidthScale);
			}
		}


		if (VelocityPredictionTime != 0)
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

