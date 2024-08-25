// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlComponent.h"
#include "PhysicsControlLog.h"
#include "PhysicsControlRecord.h"
#include "PhysicsControlComponentHelpers.h"
#include "PhysicsControlOperatorNameGeneration.h"

#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include "Physics/PhysicsInterfaceCore.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BillboardComponent.h"

#include "Engine/Engine.h"
#include "Engine/Texture2D.h"

#include "SceneManagement.h"

DECLARE_CYCLE_STAT(TEXT("PhysicsControl UpdateTargetCaches"), STAT_PhysicsControl_UpdateTargetCaches, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("PhysicsControl UpdateControls"), STAT_PhysicsControl_UpdateControls, STATGROUP_Anim);

//UE_DISABLE_OPTIMIZATION;

//======================================================================================================================
// This file contains the public member functions of UPhysicsControlComponent
//======================================================================================================================

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
	ResetControls(false);
}

//======================================================================================================================
void UPhysicsControlComponent::BeginDestroy()
{
	for (TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair : ControlRecords)
	{
		DestroyControl(
			PhysicsControlRecordPair.Key, EDestroyBehavior::KeepRecord);
	}
	ControlRecords.Empty();

	for (TPair<FName, FPhysicsBodyModifierRecord>& PhysicsBodyModifierPair : BodyModifierRecords)
	{
		DestroyBodyModifier(
			PhysicsBodyModifierPair.Key, EDestroyBehavior::KeepRecord);
	}
	BodyModifierRecords.Empty();

	Super::BeginDestroy();
}

//======================================================================================================================
void UPhysicsControlComponent::UpdateTargetCaches(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsControl_UpdateTargetCaches);
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysicsControlComponent::UpdateTargetCaches);

	// Update the skeletal mesh caches
	UpdateCachedSkeletalBoneData(DeltaTime);
}

//======================================================================================================================
void UPhysicsControlComponent::UpdateControls(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PhysicsControl_UpdateControls);
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysicsControlComponent::UpdateControls);

	ControlRecords.Compact();
	BodyModifierRecords.Compact();

	for (TPair<FName, FPhysicsControlRecord>& RecordPair : ControlRecords)
	{
		// New constraint requested when one doesn't exist
		FName ControlName = RecordPair.Key;
		FPhysicsControlRecord& Record = RecordPair.Value;
		if (!Record.ConstraintInstance)
		{
			Record.InitConstraint(this, ControlName);
		}
		ApplyControl(Record);
	}

	// Handle body modifiers
	for (TPair<FName, FPhysicsBodyModifierRecord>& BodyModifierPair : BodyModifierRecords)
	{
		FPhysicsBodyModifierRecord& BodyModifier = BodyModifierPair.Value;
		ApplyBodyModifier(BodyModifier);
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
		UE_LOG(LogPhysicsControl, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	TSet<FName> AllBones;

	// Now walk through each limb, picking up bones, ignoring any that we have already encountered.
	// This requires the setup data to have been ordered properly.
	for (const FPhysicsControlLimbSetupData& LimbSetupData : LimbSetupDatas)
	{
		FPhysicsControlLimbBones& LimbBones = Result.Add(LimbSetupData.LimbName);
		LimbBones.SkeletalMeshComponent = SkeletalMeshComponent;
		LimbBones.bCreateBodyModifiers = LimbSetupData.bCreateBodyModifiers;
		LimbBones.bCreateWorldSpaceControls = LimbSetupData.bCreateWorldSpaceControls;
		LimbBones.bCreateParentSpaceControls = LimbSetupData.bCreateParentSpaceControls;

		if (LimbSetupData.bIncludeParentBone)
		{
			LimbBones.bFirstBoneIsAdditional = true;
			const FName ParentBoneName = UE::PhysicsControl::GetPhysicalParentBone(
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
	const FName                   Set,
	const FString                 NamePrefix)
{
	const FName Name = UE::PhysicsControl::GetUniqueControlName(ParentBoneName, ChildBoneName, ControlRecords, NamePrefix);
	if (CreateNamedControl(
		Name, ParentMeshComponent, ParentBoneName, ChildMeshComponent, ChildBoneName, ControlData, ControlTarget, Set))
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
	const FName                   Set)
{
	if (FindControlRecord(Name))
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("Unable to make a Control as one with the desired name already exists"), *Name.ToString());
		return false;
	}

	if (!ChildMeshComponent)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("Unable to make a Control as the child mesh component has not been set"));
		return false;
	}

	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ParentMeshComponent))
	{
		AddSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
	}
	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ChildMeshComponent))
	{
		AddSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
	}

	FPhysicsControlRecord& NewRecord = ControlRecords.Add(
		Name, FPhysicsControlRecord(
			FPhysicsControl(ParentBoneName, ChildBoneName, ControlData), 
			ControlTarget, ParentMeshComponent, ChildMeshComponent));
	NewRecord.ResetControlPoint();

	NameRecords.AddControl(Name, Set);

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
	const FName                   Set)
{
	TArray<FName> Result;
	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		UE_LOG(LogPhysicsControl, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	int32 NumBones = SkeletalMeshComponent->GetNumBones();
	UMeshComponent* ParentMeshComponent = 
		(ControlType == EPhysicsControlType::ParentSpace) ? SkeletalMeshComponent : nullptr;

	SkeletalMeshComponent->ForEachBodyBelow(
		BoneName, bIncludeSelf, /*bSkipCustomType=*/false, 
		[
			this, PhysicsAsset, ParentMeshComponent, SkeletalMeshComponent, 
			ControlType, &ControlData, Set, &Result
		](const FBodyInstance* BI)
		{
			if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
			{
				const FName ChildBoneName = PhysicsAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;

				FName ParentBoneName;
				if (ParentMeshComponent)
				{
					ParentBoneName = UE::PhysicsControl::GetPhysicalParentBone(
						SkeletalMeshComponent, ChildBoneName);
					if (ParentBoneName.IsNone())
					{
						return;
					}
				}
				const FName ControlName = CreateControl(
					ParentMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
					ControlData, FPhysicsControlTarget(), 
					FName(GetControlTypeName(ControlType).ToString().Append("_").Append(Set.ToString())));
				if (!ControlName.IsNone())
				{
					Result.Add(ControlName);
					NameRecords.AddControl(ControlName, GetControlTypeName(ControlType));
				}
				else
				{
					UE_LOG(LogPhysicsControl, Warning, 
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
		UE_LOG(LogPhysicsControl, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	SkeletalMeshComponent->ForEachBodyBelow(
		BoneName, bIncludeSelf, /*bSkipCustomType=*/false,
		[
			this, PhysicsAsset, SkeletalMeshComponent,
			ConstraintProfile, Set, &Result, bEnabled
		](const FBodyInstance* BI)
		{
			if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
			{
				const FName ChildBoneName = PhysicsAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;

				FName ParentBoneName;
				ParentBoneName = UE::PhysicsControl::GetPhysicalParentBone(
					SkeletalMeshComponent, ChildBoneName);
				if (ParentBoneName.IsNone())
				{
					return;
				}

				FPhysicsControlData ControlData;
				// This is to match the skeletal mesh component velocity drive, which does not use the
				// target animation velocity.
				ControlData.SkeletalAnimationVelocityMultiplier = 0;
				FConstraintProfileProperties ProfileProperties;
				if (!SkeletalMeshComponent->GetConstraintProfilePropertiesOrDefault(
					ProfileProperties, ChildBoneName, ConstraintProfile))
				{
					UE_LOG(LogPhysicsControl, Warning, 
						TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
					return;
				}

				UE::PhysicsControl::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);
				ControlData.bEnabled = bEnabled;

				const FName ControlName = CreateControl(
					SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
					ControlData, FPhysicsControlTarget(), 
					FName(GetControlTypeName(EPhysicsControlType::ParentSpace).ToString().Append("_").Append(Set.ToString())));
				if (!ControlName.IsNone())
				{
					Result.Add(ControlName);
					NameRecords.AddControl(
						ControlName, GetControlTypeName(EPhysicsControlType::ParentSpace));
				}
				else
				{
					UE_LOG(LogPhysicsControl, Warning,
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
	const FName                   Set)
{
	TArray<FName> Result;
	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		UE_LOG(LogPhysicsControl, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	UMeshComponent* ParentMeshComponent =
		(ControlType == EPhysicsControlType::ParentSpace) ? SkeletalMeshComponent : nullptr;

	for (FName ChildBoneName : BoneNames)
	{
		FName ParentBoneName;
		if (ParentMeshComponent)
		{
			ParentBoneName = UE::PhysicsControl::GetPhysicalParentBone(
				SkeletalMeshComponent, ChildBoneName);
			if (ParentBoneName.IsNone())
			{
				continue;
			}
		}
		const FName ControlName = CreateControl(
			ParentMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
			ControlData, FPhysicsControlTarget(), 
			FName(GetControlTypeName(ControlType).ToString().Append("_").Append(Set.ToString())));
		if (!ControlName.IsNone())
		{
			Result.Add(ControlName);
			NameRecords.AddControl(ControlName, GetControlTypeName(ControlType));
		}
		else
		{
			UE_LOG(LogPhysicsControl, Warning, TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
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
		UE_LOG(LogPhysicsControl, Warning, TEXT("No physics asset in skeletal mesh"));
		return Result;
	}

	for (FName ChildBoneName : BoneNames)
	{
		const FName ParentBoneName = 
			UE::PhysicsControl::GetPhysicalParentBone(SkeletalMeshComponent, ChildBoneName);
		if (ParentBoneName.IsNone())
		{
			continue;
		}

		FPhysicsControlData ControlData;
		// This is to match the skeletal mesh component velocity drive, which does not use the
		// target animation velocity.
		ControlData.SkeletalAnimationVelocityMultiplier = 0;
		FConstraintProfileProperties ProfileProperties;
		if (!SkeletalMeshComponent->GetConstraintProfilePropertiesOrDefault(
			ProfileProperties, ChildBoneName, ConstraintProfile))
		{
			UE_LOG(LogPhysicsControl, Warning, 
				TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
			continue;
		}

		UE::PhysicsControl::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);
		ControlData.bEnabled = bEnabled;

		const FName ControlName = CreateControl(
			SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
			ControlData, FPhysicsControlTarget(), 
			FName(GetControlTypeName(EPhysicsControlType::ParentSpace).ToString().Append("_").Append(Set.ToString())));
		if (!ControlName.IsNone())
		{
			Result.Add(ControlName);
			NameRecords.AddControl(ControlName, GetControlTypeName(EPhysicsControlType::ParentSpace));
		}
		else
		{
			UE_LOG(LogPhysicsControl, Warning, TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
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
	UMeshComponent*                              WorldComponent,
	FName                                        WorldBoneName,
	FString                                      NamePrefix)
{
	TMap<FName, FPhysicsControlNames> Result;
	Result.Reserve(LimbBones.Num());

	for (const TPair<FName, FPhysicsControlLimbBones>& LimbBoneEntry : LimbBones)
	{
		const FName LimbName = LimbBoneEntry.Key;
		const FPhysicsControlLimbBones& BonesInLimb = LimbBoneEntry.Value;

		if (!BonesInLimb.SkeletalMeshComponent.IsValid())
		{
			UE_LOG(LogPhysicsControl, Warning, TEXT("No Skeletal mesh in limb %s"), *LimbName.ToString());
			continue;
		}

		if ((ControlType == EPhysicsControlType::WorldSpace && !BonesInLimb.bCreateWorldSpaceControls) ||
			(ControlType == EPhysicsControlType::ParentSpace && !BonesInLimb.bCreateParentSpaceControls))
		{
			continue;
		}

		USkeletalMeshComponent* ParentSkeletalMeshComponent =
			(ControlType == EPhysicsControlType::ParentSpace) ? BonesInLimb.SkeletalMeshComponent.Get() : nullptr;

		const int32 NumBonesInLimb = BonesInLimb.BoneNames.Num();

		FPhysicsControlNames& LimbResult = Result.Add(LimbName);
		LimbResult.Names.Reserve(NumBonesInLimb);
		AllControls.Names.Reserve(AllControls.Names.Num() + NumBonesInLimb);

		FString SetName = 
			NamePrefix + GetControlTypeName(ControlType).ToString().Append("_").Append(LimbName.ToString());

		for (int32 BoneIndex = 0 ; BoneIndex != NumBonesInLimb ; ++BoneIndex)
		{
			// Don't create the parent space control if it's the first bone in a limb that had bIncludeParentBone
			if (BoneIndex == 0 && BonesInLimb.bFirstBoneIsAdditional && ControlType == EPhysicsControlType::ParentSpace)
			{
				continue;
			}

			FName ChildBoneName = BonesInLimb.BoneNames[BoneIndex];

			FName ParentBoneName;
			if (ParentSkeletalMeshComponent)
			{
				ParentBoneName = UE::PhysicsControl::GetPhysicalParentBone(
					ParentSkeletalMeshComponent, ChildBoneName);
				if (ParentBoneName.IsNone())
				{
					continue;
				}
			}

			UMeshComponent* ParentMeshComponent = ParentSkeletalMeshComponent;
			if (!ParentMeshComponent && WorldComponent)
			{
				ParentMeshComponent = WorldComponent;
				ParentBoneName = WorldBoneName;
			}

			const FName ControlName = CreateControl(
				ParentMeshComponent, ParentBoneName, BonesInLimb.SkeletalMeshComponent.Get(), ChildBoneName,
				ControlData, FPhysicsControlTarget(), FName(SetName));

			if (!ControlName.IsNone())
			{
				LimbResult.Names.Add(ControlName);
				AllControls.Names.Add(ControlName);
				NameRecords.AddControl(ControlName, GetControlTypeName(ControlType));
			}
			else
			{
				UE_LOG(LogPhysicsControl, Warning, 
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

		USkeletalMeshComponent* SkeletalMeshComponent = BonesInLimb.SkeletalMeshComponent.Get();
		if (!SkeletalMeshComponent)
		{
			UE_LOG(LogPhysicsControl, Warning, TEXT("No Skeletal mesh in limb %s"), *LimbName.ToString());
			continue;
		}
		UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
		if (!PhysicsAsset)
		{
			UE_LOG(LogPhysicsControl, Warning, TEXT("No physics asset in skeletal mesh"));
			return Result;
		}

		const int32 NumBonesInLimb = BonesInLimb.BoneNames.Num();

		FPhysicsControlNames& LimbResult = Result.Add(LimbName);
		LimbResult.Names.Reserve(NumBonesInLimb);
		AllControls.Names.Reserve(AllControls.Names.Num() + NumBonesInLimb);

		for (int32 BoneIndex = 0; BoneIndex != NumBonesInLimb; ++BoneIndex)
		{
			// Don't create the parent space control if it's the first bone in a limb that had bIncludeParentBone
			if (BoneIndex == 0 && BonesInLimb.bFirstBoneIsAdditional)
			{
				continue; 
			}

			const FName ChildBoneName = BonesInLimb.BoneNames[BoneIndex];
			const FName ParentBoneName = 
				UE::PhysicsControl::GetPhysicalParentBone(SkeletalMeshComponent, ChildBoneName);
			if (ParentBoneName.IsNone())
			{
				continue;
			}

			FPhysicsControlData ControlData;
			// This is to match the skeletal mesh component velocity drive, which does not use the
			// target animation velocity.
			ControlData.SkeletalAnimationVelocityMultiplier = 0;

			FConstraintProfileProperties ProfileProperties;
			if (!SkeletalMeshComponent->GetConstraintProfilePropertiesOrDefault(
				ProfileProperties, ChildBoneName, ConstraintProfile))
			
			{
				UE_LOG(LogPhysicsControl, Warning, 
					TEXT("Failed get constraint profile for %s"), *ChildBoneName.ToString());
				continue;
			}

			UE::PhysicsControl::ConvertConstraintProfileToControlData(ControlData, ProfileProperties);
			ControlData.bEnabled = bEnabled;

			const FName ControlName = CreateControl(
				SkeletalMeshComponent, ParentBoneName, SkeletalMeshComponent, ChildBoneName,
				ControlData, FPhysicsControlTarget(), 
				FName(GetControlTypeName(EPhysicsControlType::ParentSpace).ToString().Append("_").Append(LimbName.ToString())));
			if (!ControlName.IsNone())
			{
				LimbResult.Names.Add(ControlName);
				AllControls.Names.Add(ControlName);
				NameRecords.AddControl(ControlName, GetControlTypeName(EPhysicsControlType::ParentSpace));
			}
			else
			{
				UE_LOG(LogPhysicsControl, Warning, 
					TEXT("Failed to make control for %s"), *ChildBoneName.ToString());
			}
		}
	}
	return Result;
}

//======================================================================================================================
bool UPhysicsControlComponent::DestroyControl(const FName Name)
{
	return DestroyControl(Name, EDestroyBehavior::RemoveRecord);
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyControls(const TArray<FName>& Names)
{
	for (FName Name : Names)
	{
		DestroyControl(Name, EDestroyBehavior::RemoveRecord);
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
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData.bEnabled = bEnable;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlEnabled - invalid name %s"), *Name.ToString());
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
bool UPhysicsControlComponent::SetControlParent(
	const FName     Name,
	UMeshComponent* ParentMeshComponent,
	const FName     ParentBoneName)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		if (USkeletalMeshComponent* SkeletalMeshComponent = 
			Cast<USkeletalMeshComponent>(Record->ParentMeshComponent.Get()))
		{
			RemoveSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		}

		if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ParentMeshComponent))
		{
			AddSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		}

		Record->ParentMeshComponent = ParentMeshComponent;
		Record->PhysicsControl.ParentBoneName = ParentBoneName;
		return Record->InitConstraint(this, Name);
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlParent - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlParents(
		const TArray<FName>& Names,
		UMeshComponent*      ParentMeshComponent,
		const FName          ParentBoneName)
{
	for (FName Name : Names)
	{
		SetControlParent(Name, ParentMeshComponent, ParentBoneName);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlParentsInSet(
	const FName     SetName,
	UMeshComponent* ParentMeshComponent,
	const FName     ParentBoneName)
{
	SetControlParents(GetControlNamesInSet(SetName), ParentMeshComponent, ParentBoneName);
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetControlData(
	const FName               Name, 
	const FPhysicsControlData ControlData)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData = ControlData;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlData - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlDatas(
	const TArray<FName>&      Names, 
	const FPhysicsControlData ControlData)
{
	for (FName Name : Names)
	{
		SetControlData(Name, ControlData);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlDatasInSet(
	const FName               SetName,
	const FPhysicsControlData ControlData)
{
	SetControlDatas(GetControlNamesInSet(SetName), ControlData);
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetControlSparseData(
	const FName                     Name, 
	const FPhysicsControlSparseData ControlData)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData.UpdateFromSparseData(ControlData);
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlData - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlSparseDatas(
	const TArray<FName>&            Names, 
	const FPhysicsControlSparseData ControlData)
{
	for (FName Name : Names)
	{
		SetControlSparseData(Name, ControlData);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlSparseDatasInSet(
	const FName                     SetName,
	const FPhysicsControlSparseData ControlData)
{
	SetControlSparseDatas(GetControlNamesInSet(SetName), ControlData);
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetControlMultiplier(
	const FName                      Name, 
	const FPhysicsControlMultiplier  ControlMultiplier, 
	const bool                       bEnableControl)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlMultiplier = ControlMultiplier;
		if (bEnableControl)
		{
			Record->PhysicsControl.ControlData.bEnabled = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlMultiplier - invalid name %s"), *Name.ToString());
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
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetControlSparseMultiplier(
	const FName                            Name,
	const FPhysicsControlSparseMultiplier  ControlMultiplier,
	const bool                             bEnableControl)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlMultiplier.UpdateFromSparseData(ControlMultiplier);
		if (bEnableControl)
		{
			Record->PhysicsControl.ControlData.bEnabled = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlSparseMultiplier - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlSparseMultipliers(
	const TArray<FName>&                  Names,
	const FPhysicsControlSparseMultiplier ControlMultiplier,
	const bool                            bEnableControl)
{
	for (FName Name : Names)
	{
		SetControlSparseMultiplier(Name, ControlMultiplier, bEnableControl);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetControlSparseMultipliersInSet(
	const FName                           SetName,
	const FPhysicsControlSparseMultiplier ControlMultiplier,
	const bool                            bEnableControl)
{
	SetControlSparseMultipliers(GetControlNamesInSet(SetName), ControlMultiplier, bEnableControl);
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
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData.LinearStrength = Strength;
		Record->PhysicsControl.ControlData.LinearDampingRatio = DampingRatio;
		Record->PhysicsControl.ControlData.LinearExtraDamping = ExtraDamping;
		Record->PhysicsControl.ControlData.MaxForce = MaxForce;
		if (bEnableControl)
		{
			Record->PhysicsControl.ControlData.bEnabled = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlLinearData - invalid name %s"), *Name.ToString());
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
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData.AngularStrength = Strength;
		Record->PhysicsControl.ControlData.AngularDampingRatio = DampingRatio;
		Record->PhysicsControl.ControlData.AngularExtraDamping = ExtraDamping;
		Record->PhysicsControl.ControlData.MaxTorque = MaxTorque;
		if (bEnableControl)
		{
			Record->PhysicsControl.ControlData.bEnabled = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlAngularData - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlPoint(const FName Name, const FVector Position)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->PhysicsControl.ControlData.bUseCustomControlPoint = true;
		Record->PhysicsControl.ControlData.CustomControlPoint = Position;
		Record->UpdateConstraintControlPoint();
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlPoint - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::ResetControlPoint(const FName Name)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->ResetControlPoint();
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("ResetControlPoint - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlTarget(
	const FName                 Name, 
	const FPhysicsControlTarget ControlTarget, 
	const bool                  bEnableControl)
{
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		Record->ControlTarget = ControlTarget;
		if (bEnableControl)
		{
			Record->PhysicsControl.ControlData.bEnabled = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlTarget - invalid name %s"), *Name.ToString());
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
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		SetControlTargetPosition(
			Name, Position, VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
		SetControlTargetOrientation(
			Name, Orientation, VelocityDeltaTime, bEnableControl, bApplyControlPointToTarget);
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlTargetPositionAndOrientation - invalid name %s"), *Name.ToString());
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
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		if (VelocityDeltaTime != 0)
		{
			Record->ControlTarget.TargetVelocity =
				(Position - Record->ControlTarget.TargetPosition) / VelocityDeltaTime;
		}
		else
		{
			Record->ControlTarget.TargetVelocity = FVector::ZeroVector;
		}
		Record->ControlTarget.TargetPosition = Position;
		Record->ControlTarget.bApplyControlPointToTarget = bApplyControlPointToTarget;
		if (bEnableControl)
		{
			Record->PhysicsControl.ControlData.bEnabled = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlTargetPosition - invalid name %s"), *Name.ToString());
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
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		if (AngularVelocityDeltaTime != 0)
		{
			FQuat OldQ = Record->ControlTarget.TargetOrientation.Quaternion();
			const FQuat OrientationQ = Orientation.Quaternion();
			OldQ.EnforceShortestArcWith(OrientationQ);
			// Note that quats multiply in the opposite order to TMs
			const FQuat DeltaQ = OrientationQ * OldQ.Inverse();
			Record->ControlTarget.TargetAngularVelocity =
				DeltaQ.ToRotationVector() / (UE_TWO_PI * AngularVelocityDeltaTime);
		}
		else
		{
			Record->ControlTarget.TargetAngularVelocity = FVector::ZeroVector;
		}
		Record->ControlTarget.TargetOrientation = Orientation;
		Record->ControlTarget.bApplyControlPointToTarget = bApplyControlPointToTarget;
		if (bEnableControl)
		{
			Record->PhysicsControl.ControlData.bEnabled = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlTargetOrientation - invalid name %s"), *Name.ToString());
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
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlTargetPositionsFromArray - names and positions arrays sizes do not match"));
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
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlTargetOrientationsFromArray - names and orientations arrays sizes do not match"));
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
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlTargetPositionsAndOrientationsFromArray - names and positions/orientation arrays sizes do not match"));
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
	FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		const FTransform ParentTM(ParentOrientation, ParentPosition, FVector::One());
		const FTransform ChildTM(ChildOrientation, ChildPosition, FVector::One());

		const FTransform OffsetTM = ChildTM * ParentTM.Inverse();
		const FVector Position = OffsetTM.GetTranslation();
		const FQuat OrientationQ = OffsetTM.GetRotation();

		if (VelocityDeltaTime != 0)
		{
			FQuat OldQ = Record->ControlTarget.TargetOrientation.Quaternion();
			OldQ.EnforceShortestArcWith(OrientationQ);
			// Note that quats multiply in the opposite order to TMs
			FQuat DeltaQ = OrientationQ * OldQ.Inverse();
			Record->ControlTarget.TargetAngularVelocity =
				DeltaQ.ToRotationVector() / (UE_TWO_PI * VelocityDeltaTime);

			Record->ControlTarget.TargetVelocity =
				(Position - Record->ControlTarget.TargetPosition) / VelocityDeltaTime;
		}
		else
		{
			Record->ControlTarget.TargetAngularVelocity = FVector::ZeroVector;
			Record->ControlTarget.TargetVelocity = FVector::ZeroVector;
		}
		Record->ControlTarget.TargetOrientation = OrientationQ.Rotator();
		Record->ControlTarget.TargetPosition = Position;
		Record->ControlTarget.bApplyControlPointToTarget = true;
		if (bEnableControl)
		{
			Record->PhysicsControl.ControlData.bEnabled = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlTargetPoses - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetControlUseSkeletalAnimation(
	const FName Name,
	const bool  bUseSkeletalAnimation,
	const float SkeletalAnimationVelocityMultiplier)
{
	FPhysicsControl* PhysicsControl = FindControl(Name);
	if (PhysicsControl)
	{
		PhysicsControl->ControlData.bUseSkeletalAnimation = bUseSkeletalAnimation;
		PhysicsControl->ControlData.SkeletalAnimationVelocityMultiplier = SkeletalAnimationVelocityMultiplier;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlUseSkeletalAnimation - invalid name %s"), *Name.ToString());
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
bool UPhysicsControlComponent::SetControlDisableCollision(const FName Name, const bool bDisableCollision)
{
	FPhysicsControl* PhysicsControl = FindControl(Name);
	if (PhysicsControl)
	{
		PhysicsControl->ControlData.bDisableCollision = bDisableCollision;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetControlDisableCollision - invalid name %s"), *Name.ToString());
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
	const FPhysicsControl* PhysicsControl = FindControl(Name);
	if (PhysicsControl)
	{
		ControlData = PhysicsControl->ControlData;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetControlData - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControlMultiplier(const FName Name, FPhysicsControlMultiplier& ControlMultiplier) const
{
	const FPhysicsControl* PhysicsControl = FindControl(Name);
	if (PhysicsControl)
	{
		ControlMultiplier = PhysicsControl->ControlMultiplier;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetControlMultiplier - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControlTarget(const FName Name, FPhysicsControlTarget& ControlTarget) const
{
	const FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		ControlTarget = Record->ControlTarget;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetControlTarget - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetControlEnabled(const FName Name) const
{
	const FPhysicsControlRecord* Record = FindControlRecord(Name);
	if (Record)
	{
		return Record->PhysicsControl.IsEnabled();
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetControlEnabled - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
FName UPhysicsControlComponent::CreateBodyModifier(
	UMeshComponent*                   MeshComponent,
	const FName                       BoneName,
	const FName                       Set,
	const FPhysicsControlModifierData BodyModifierData)
{
	TSet<FName> Keys;
	BodyModifierRecords.GetKeys(Keys);
	const FName Name = UE::PhysicsControl::GetUniqueBodyModifierName(BoneName, Keys, TEXT(""));
	if (CreateNamedBodyModifier(Name, MeshComponent, BoneName, Set, BodyModifierData))
	{
		return Name;
	}
	return FName();
}

//======================================================================================================================
bool UPhysicsControlComponent::CreateNamedBodyModifier(
	const FName                       Name,
	UMeshComponent*                   MeshComponent,
	const FName                       BoneName,
	const FName                       Set,
	const FPhysicsControlModifierData BodyModifierData)
{
	if (FindBodyModifierRecord(Name))
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("CreateNamedBodyModifier - modifier with name %s already exists"), *Name.ToString());
		return false;
	}

	if (!MeshComponent)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("Unable to make a PhysicsBodyModifier as the mesh component has not been set"));
		return false;
	}

	FPhysicsBodyModifierRecord& Modifier = BodyModifierRecords.Add(
		Name, FPhysicsBodyModifierRecord(MeshComponent, BoneName, BodyModifierData));

	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(MeshComponent);
	if (SkeletalMeshComponent)
	{
		AddSkeletalMeshReferenceForCaching(SkeletalMeshComponent);
		AddSkeletalMeshReferenceForModifier(SkeletalMeshComponent);
	}

	NameRecords.AddBodyModifier(Name, Set);

	return true;
}

//======================================================================================================================
TArray<FName> UPhysicsControlComponent::CreateBodyModifiersFromSkeletalMeshBelow(
	USkeletalMeshComponent*           SkeletalMeshComponent,
	const FName                       BoneName,
	const bool                        bIncludeSelf,
	const FName                       Set,
	const FPhysicsControlModifierData BodyModifierData)
{
	TArray<FName> Result;
	UPhysicsAsset* PhysicsAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("CreateBodyModifiersFromSkeletalMeshBelow - No physics asset available"));
		return Result;
	}

	SkeletalMeshComponent->ForEachBodyBelow(
		BoneName, bIncludeSelf, /*bSkipCustomType=*/false,
		[this, PhysicsAsset, SkeletalMeshComponent, Set, BodyModifierData, &Result](const FBodyInstance* BI)
		{
			if (USkeletalBodySetup* BodySetup = Cast<USkeletalBodySetup>(BI->BodySetup.Get()))
			{
				const FName BoneName = PhysicsAsset->SkeletalBodySetups[BI->InstanceBodyIndex]->BoneName;
				const FName BodyModifierName = CreateBodyModifier(
					SkeletalMeshComponent, BoneName, Set, BodyModifierData);
				Result.Add(BodyModifierName);
			}
		});

	return Result;
}

//======================================================================================================================
TMap<FName, FPhysicsControlNames> UPhysicsControlComponent::CreateBodyModifiersFromLimbBones(
	FPhysicsControlNames&                        AllBodyModifiers,
	const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
	const FPhysicsControlModifierData            BodyModifierData)
{
	TMap<FName, FPhysicsControlNames> Result;
	Result.Reserve(LimbBones.Num());

	for (const TPair<FName, FPhysicsControlLimbBones>& LimbBoneEntry : LimbBones)
	{
		const FName LimbName = LimbBoneEntry.Key;
		const FPhysicsControlLimbBones& BonesInLimb = LimbBoneEntry.Value;

		if (!BonesInLimb.SkeletalMeshComponent.Get())
		{
			UE_LOG(LogPhysicsControl, Warning, TEXT("No Skeletal mesh in limb %s"), *LimbName.ToString());
			continue;
		}

		const int32 NumBonesInLimb = BonesInLimb.BoneNames.Num();

		FPhysicsControlNames& LimbResult = Result.Add(LimbName);
		LimbResult.Names.Reserve(NumBonesInLimb);
		AllBodyModifiers.Names.Reserve(AllBodyModifiers.Names.Num() + NumBonesInLimb);

		for (const FName BoneName : BonesInLimb.BoneNames)
		{
			const FName BodyModifierName = CreateBodyModifier(
				BonesInLimb.SkeletalMeshComponent.Get(), BoneName, LimbName, BodyModifierData);
			if (!BodyModifierName.IsNone())
			{
				LimbResult.Names.Add(BodyModifierName);
				AllBodyModifiers.Names.Add(BodyModifierName);
			}
			else
			{
				UE_LOG(LogPhysicsControl, Warning, 
					TEXT("Failed to make body modifier for %s"), *BoneName.ToString());
			}
		}
	}
	return Result;
}

//======================================================================================================================
bool UPhysicsControlComponent::DestroyBodyModifier(const FName Name)
{
	return DestroyBodyModifier(Name, EDestroyBehavior::RemoveRecord);
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyBodyModifiers(const TArray<FName>& Names)
{
	for (FName Name : Names)
	{
		DestroyBodyModifier(Name, EDestroyBehavior::RemoveRecord);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::DestroyBodyModifiersInSet(const FName SetName)
{
	TArray<FName> Names = GetBodyModifierNamesInSet(SetName);
	DestroyBodyModifiers(Names);
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetBodyModifierData(
	const FName                       Name,
	const FPhysicsControlModifierData ModifierData)
{
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->BodyModifier.ModifierData = ModifierData;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierData - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifierDatas(
	const TArray<FName>&              Names,
	const FPhysicsControlModifierData ModifierData)
{
	for (FName Name : Names)
	{
		SetBodyModifierData(Name, ModifierData);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifierDatasInSet(
	const FName                       SetName,
	const FPhysicsControlModifierData ModifierData)
{
	SetBodyModifierDatas(GetControlNamesInSet(SetName), ModifierData);
}

//======================================================================================================================
// Note - some params passed by value to allow them to be unconnected and enable inline BP editing
bool UPhysicsControlComponent::SetBodyModifierSparseData(
	const FName                             Name,
	const FPhysicsControlModifierSparseData ModifierData)
{
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->BodyModifier.ModifierData.UpdateFromSparseData(ModifierData);
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierData - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifierSparseDatas(
	const TArray<FName>&                    Names,
	const FPhysicsControlModifierSparseData ModifierData)
{
	for (FName Name : Names)
	{
		SetBodyModifierSparseData(Name, ModifierData);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::SetBodyModifierSparseDatasInSet(
	const FName                             SetName,
	const FPhysicsControlModifierSparseData ModifierData)
{
	SetBodyModifierSparseDatas(GetControlNamesInSet(SetName), ModifierData);
}


//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierKinematicTarget(
	const FName    Name,
	const FVector  KinematicTargetPosition,
	const FRotator KinematicTargetOrienation,
	const bool     bMakeKinematic)
{
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->KinematicTargetPosition = KinematicTargetPosition;
		Record->KinematicTargetOrientation = KinematicTargetOrienation.Quaternion();
		if (bMakeKinematic)
		{
			Record->BodyModifier.ModifierData.MovementType = EPhysicsMovementType::Kinematic;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierKinematicTarget - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::SetBodyModifierMovementType(
	const FName                Name,
	const EPhysicsMovementType MovementType)
{
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->BodyModifier.ModifierData.MovementType = MovementType;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierMovementType - invalid name %s"), *Name.ToString());
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
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->BodyModifier.ModifierData.CollisionType = CollisionType;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierCollisionType - invalid name %s"), *Name.ToString());
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
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->BodyModifier.ModifierData.GravityMultiplier = GravityMultiplier;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierGravityMultiplier - invalid name %s"), *Name.ToString());
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
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->BodyModifier.ModifierData.PhysicsBlendWeight = PhysicsBlendWeight;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierPhysicsBlendWeight - invalid name %s"), *Name.ToString());
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
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->BodyModifier.ModifierData.bUseSkeletalAnimation = bUseSkeletalAnimation;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierUseSkeletalAnimation - invalid name %s"), *Name.ToString());
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
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		Record->BodyModifier.ModifierData.bUpdateKinematicFromSimulation = bUpdateKinematicFromSimulation;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetBodyModifierUpdateKinematicFromSimulation - invalid name %s"), *Name.ToString());
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
	const FPhysicsControlData                   ParentSpaceControlData,
	const FPhysicsControlModifierData           BodyModifierData,
	UMeshComponent*                             WorldComponent,
	FName                                       WorldBoneName)
{
	TMap<FName, FPhysicsControlLimbBones> LimbBones = 
		GetLimbBonesFromSkeletalMesh(SkeletalMeshComponent, LimbSetupData);

	LimbWorldSpaceControls = CreateControlsFromLimbBones(
		AllWorldSpaceControls, LimbBones, EPhysicsControlType::WorldSpace, 
		WorldSpaceControlData, WorldComponent, WorldBoneName);

	LimbParentSpaceControls = CreateControlsFromLimbBones(
		AllParentSpaceControls, LimbBones, EPhysicsControlType::ParentSpace,
		ParentSpaceControlData);

	LimbBodyModifiers = CreateBodyModifiersFromLimbBones(AllBodyModifiers, LimbBones, BodyModifierData);
}

//======================================================================================================================
void UPhysicsControlComponent::CreateControlsAndBodyModifiersFromControlProfileAsset(
	USkeletalMeshComponent* SkeletalMeshComponent,
	UMeshComponent*         WorldComponent,
	FName                   WorldBoneName)
{
	if (!PhysicsControlProfileAsset.IsValid())
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("CreateControlsAndBodyModifiersFromControlProfile - unable to get/load the control profile asset"));
		return;
	}

	FPhysicsControlNames AllWorldSpaceControls;
	TMap<FName, FPhysicsControlNames> LimbWorldSpaceControls;
	FPhysicsControlNames AllParentSpaceControls;
	TMap<FName, FPhysicsControlNames> LimbParentSpaceControls;
	FPhysicsControlNames AllBodyModifiers;
	TMap<FName, FPhysicsControlNames> LimbBodyModifiers;

	CreateControlsAndBodyModifiersFromLimbBones(
		AllWorldSpaceControls, LimbWorldSpaceControls, AllParentSpaceControls, LimbParentSpaceControls, 
		AllBodyModifiers, LimbBodyModifiers,
		SkeletalMeshComponent,
		PhysicsControlProfileAsset->CharacterSetupData.LimbSetupData,
		PhysicsControlProfileAsset->CharacterSetupData.DefaultWorldSpaceControlData,
		PhysicsControlProfileAsset->CharacterSetupData.DefaultParentSpaceControlData,
		PhysicsControlProfileAsset->CharacterSetupData.DefaultBodyModifierData,
		WorldComponent,
		WorldBoneName);

	// Create additional controls
	for (const TPair<FName, FPhysicsControlCreationData>& ControlPair : 
		PhysicsControlProfileAsset->AdditionalControlsAndModifiers.Controls)
	{
		FName ControlName = ControlPair.Key;
		const FPhysicsControlCreationData& ControlCreationData = ControlPair.Value;
		if (CreateNamedControl(ControlName,
			!ControlCreationData.Control.ParentBoneName.IsNone() 
			? SkeletalMeshComponent : nullptr, ControlCreationData.Control.ParentBoneName,
			SkeletalMeshComponent, ControlCreationData.Control.ChildBoneName,
			ControlCreationData.Control.ControlData, FPhysicsControlTarget(), FName()))
		{
			for (FName SetName : ControlCreationData.Sets)
			{
				NameRecords.AddControl(ControlName, SetName);
			}
		}
	}

	// Create additional modifiers
	for (const TPair<FName, FPhysicsBodyModifierCreationData>& ModifierPair : 
		PhysicsControlProfileAsset->AdditionalControlsAndModifiers.Modifiers)
	{
		FName ModifierName = ModifierPair.Key;
		const FPhysicsBodyModifierCreationData& ModifierCreationData = ModifierPair.Value;
		if (CreateNamedBodyModifier(ModifierName,
			SkeletalMeshComponent, ModifierCreationData.Modifier.BoneName,
			FName(), ModifierCreationData.Modifier.ModifierData))
		{
			for (FName SetName : ModifierCreationData.Sets)
			{
				NameRecords.AddBodyModifier(ModifierName, SetName);
			}
		}
	}

	// Create any additional sets that have been requested
	UE::PhysicsControl::CreateAdditionalSets(
		PhysicsControlProfileAsset->AdditionalSets, BodyModifierRecords, ControlRecords, NameRecords);

	for (FPhysicsControlControlAndModifierUpdates& Updates : PhysicsControlProfileAsset->InitialControlAndModifierUpdates)
	{
		ApplyControlAndModifierUpdates(Updates);
	}
}

//======================================================================================================================
void UPhysicsControlComponent::InvokeControlProfile(FName ProfileName)
{
	if (!PhysicsControlProfileAsset.IsValid())
	{
		if (bWarnAboutInvalidNames)
		{
			UE_LOG(LogPhysicsControl, Warning,
				TEXT("InvokeControlProfile - control profile asset is invalid or missing"));
		}
		return;
	}

	const FPhysicsControlControlAndModifierUpdates* ControlAndModifierUpdates =
		PhysicsControlProfileAsset->Profiles.Find(ProfileName);

	if (!ControlAndModifierUpdates)
	{
		if (bWarnAboutInvalidNames)
		{
			UE_LOG(LogPhysicsControl, Warning,
				TEXT("InvokeControlProfile - control profile %s not found"), *ProfileName.ToString());
		}
		return;
	}

	ApplyControlAndModifierUpdates(*ControlAndModifierUpdates);

	return;
}

//======================================================================================================================
void UPhysicsControlComponent::ApplyControlAndModifierUpdates(
	const FPhysicsControlControlAndModifierUpdates& ControlAndModifierUpdates)
{
	for (const FPhysicsControlNamedControlParameters& ControlParameters : ControlAndModifierUpdates.ControlUpdates)
	{
		TArray<FName> Names = ExpandName(ControlParameters.Name, NameRecords.ControlSets);
		for (FName Name : Names)
		{
			const FPhysicsControlSparseData& ControlData = ControlParameters.Data;
			if (FPhysicsControlRecord* ControlRecord = ControlRecords.Find(Name))
			{
				ControlRecord->PhysicsControl.ControlData.UpdateFromSparseData(ControlData);
			}
			else
			{
				if (bWarnAboutInvalidNames)
				{
					UE_LOG(LogPhysicsControl, Warning,
						TEXT("ApplyControlAndModifierUpdates: Failed to find control with name %s"), *Name.ToString());
				}
			}
		}
	}

	for (const FPhysicsControlNamedControlMultiplierParameters& ControlMultiplierParameters :
		ControlAndModifierUpdates.ControlMultiplierUpdates)
	{
		TArray<FName> Names = ExpandName(ControlMultiplierParameters.Name, NameRecords.ControlSets);
		for (FName Name : Names)
		{
			const FPhysicsControlSparseMultiplier& Multiplier = ControlMultiplierParameters.Data;
			if (FPhysicsControlRecord* ControlRecord = ControlRecords.Find(Name))
			{
				ControlRecord->PhysicsControl.ControlMultiplier.UpdateFromSparseData(Multiplier);
			}
			else
			{
				if (bWarnAboutInvalidNames)
				{
					UE_LOG(LogPhysicsControl, Warning,
						TEXT("ApplyControlAndModifierUpdates: Failed to find control with name %s"), *Name.ToString());
				}
			}
		}
	}

	for (const FPhysicsControlNamedModifierParameters& ModifierParameters : ControlAndModifierUpdates.ModifierUpdates)
	{
		TArray<FName> Names = ExpandName(ModifierParameters.Name, NameRecords.BodyModifierSets);
		for (FName Name : Names)
		{
			const FPhysicsControlModifierSparseData& ModifierData = ModifierParameters.Data;
			if (FPhysicsBodyModifierRecord* Record = BodyModifierRecords.Find(Name))
			{
				Record->BodyModifier.ModifierData.UpdateFromSparseData(ModifierData);
			}
			else
			{
				UE_LOG(LogPhysicsControl, Warning,
					TEXT("InvokeControlProfile: Failed to find modifier with name %s"), *Name.ToString());
			}
		}
	}
}

//======================================================================================================================
void UPhysicsControlComponent::AddControlToSet(
	FPhysicsControlNames& NewSet, 
	const FName           Control, 
	const FName           SetName)
{
	NameRecords.AddControl(Control, SetName);
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
		NameRecords.AddControl(Control, SetName);
	}
	NewSet.Names = GetControlNamesInSet(SetName);
}

//======================================================================================================================
const TArray<FName>& UPhysicsControlComponent::GetControlNamesInSet(const FName SetName) const
{
	return NameRecords.GetControlNamesInSet(SetName);
}

//======================================================================================================================
const TArray<FName>& UPhysicsControlComponent::GetAllBodyModifierNames() const
{
	return GetBodyModifierNamesInSet("All");
}

//======================================================================================================================
const TArray<FName>& UPhysicsControlComponent::GetBodyModifierNamesInSet(const FName SetName) const
{
	return NameRecords.GetBodyModifierNamesInSet(SetName);
}

//======================================================================================================================
void UPhysicsControlComponent::AddBodyModifierToSet(
	FPhysicsControlNames& NewSet, 
	const FName           BodyModifier, 
	const FName           SetName)
{
	NameRecords.AddBodyModifier(BodyModifier, SetName);
	NewSet.Names = GetBodyModifierNamesInSet(SetName);
}


//======================================================================================================================
void UPhysicsControlComponent::AddBodyModifiersToSet(
	FPhysicsControlNames& NewSet, 
	const TArray<FName>&  InBodyModifiers, 
	const FName           SetName)
{
	for (FName BodyModifier : InBodyModifiers)
	{
		NameRecords.AddBodyModifier(BodyModifier, SetName);
	}
	NewSet.Names = GetBodyModifierNamesInSet(SetName);
}

//======================================================================================================================
TArray<FName> UPhysicsControlComponent::GetSetsContainingControl(const FName Control) const
{
	TArray<FName> Result;
	for (const TPair<FName, TArray<FName>>& ControlSetPair : NameRecords.ControlSets)
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
	for (const TPair<FName, TArray<FName>>& BodyModifierSetPair : NameRecords.BodyModifierSets)
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
		if (GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
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
		if (GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
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
		if (GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
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
		if (GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
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
		if (GetBoneData(BoneData, SkeletalMeshComponent, BoneName))
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
	const FName                   Name)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (GetBoneData(BoneData, SkeletalMeshComponent, Name))
	{
		return FTransform(BoneData.Orientation, BoneData.Position);
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetCachedBoneTransform - invalid name %s"), *Name.ToString());
	}
	return FTransform();
}

//======================================================================================================================
FVector UPhysicsControlComponent::GetCachedBonePosition(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   Name)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (GetBoneData(BoneData, SkeletalMeshComponent, Name))
	{
		return BoneData.Position;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetCachedBonePosition - invalid name %s"), *Name.ToString());
	}
	return FVector::ZeroVector;
}

//======================================================================================================================
FRotator UPhysicsControlComponent::GetCachedBoneOrientation(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   Name)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (GetBoneData(BoneData, SkeletalMeshComponent, Name))
	{
		return BoneData.Orientation.Rotator();
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetCachedBoneOrientation - invalid name %s"), *Name.ToString());
	}
	return FRotator::ZeroRotator;
}

//======================================================================================================================
FVector UPhysicsControlComponent::GetCachedBoneVelocity(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   Name)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (GetBoneData(BoneData, SkeletalMeshComponent, Name))
	{
		return BoneData.Velocity;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetCachedBoneVelocity - invalid name %s"), *Name.ToString());
	}
	return FVector::Zero();
}

//======================================================================================================================
FVector UPhysicsControlComponent::GetCachedBoneAngularVelocity(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   Name)
{
	FCachedSkeletalMeshData::FBoneData BoneData;
	if (GetBoneData(BoneData, SkeletalMeshComponent, Name))
	{
		return BoneData.AngularVelocity;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("GetCachedBoneAngularVelocity - invalid name %s"), *Name.ToString());
	}
	return FVector::Zero();
}

//======================================================================================================================
bool UPhysicsControlComponent::SetCachedBoneData(
	const USkeletalMeshComponent* SkeletalMeshComponent,
	const FName                   Name,
	const FTransform&             TM,
	const FVector                 Velocity,
	const FVector                 AngularVelocity)
{
	FCachedSkeletalMeshData::FBoneData* BoneData;
	if (GetModifiableBoneData(BoneData, SkeletalMeshComponent, Name))
	{
		BoneData->Position = TM.GetLocation();
		BoneData->Orientation = TM.GetRotation();
		BoneData->Velocity = Velocity;
		BoneData->AngularVelocity = AngularVelocity;
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("SetCachedBoneData - invalid name %s"), *Name.ToString());
	}
	return false;
}

//======================================================================================================================
bool UPhysicsControlComponent::ResetBodyModifierToCachedBoneTransform(
	const FName                        Name,
	const EResetToCachedTargetBehavior Behavior)
{
	FPhysicsBodyModifierRecord* Record = FindBodyModifierRecord(Name);
	if (Record)
	{
		if (Behavior == EResetToCachedTargetBehavior::ResetImmediately)
		{
			ResetToCachedTarget(*Record);
		}
		else
		{
			Record->bResetToCachedTarget = true;
		}
		return true;
	}
	if (bWarnAboutInvalidNames)
	{
		UE_LOG(LogPhysicsControl, Warning,
			TEXT("ResetBodyModifierToCachedBoneTransform - invalid name %s"), *Name.ToString());
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

//======================================================================================================================
bool UPhysicsControlComponent::GetControlExists(const FName Name) const
{
	return FindControlRecord(Name) != nullptr;
}

//======================================================================================================================
bool UPhysicsControlComponent::GetBodyModifierExists(const FName Name) const
{
	return FindBodyModifierRecord(Name) != nullptr;
}

#if WITH_EDITOR

//======================================================================================================================
void UPhysicsControlComponent::OnRegister()
{
	Super::OnRegister();

	if (SpriteComponent)
	{
		SpriteComponent->SetSprite(LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/S_KBSJoint.S_KBSJoint")));
		SpriteComponent->SpriteInfo.Category = TEXT("Physics");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Physics", "Physics");
	}
}

//======================================================================================================================
void UPhysicsControlComponent::DebugDraw(FPrimitiveDrawInterface* PDI) const
{
	// Draw gizmos
	if (bShowDebugVisualization && VisualizationSizeScale > 0)
	{
		for (const TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair : 
			ControlRecords)
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
			ControlRecords)
		{
			const FName Name = PhysicsControlRecordPair.Key;

			if (Name.ToString().Contains(DebugControlDetailFilter))
			{
				const FPhysicsControlRecord& Record = PhysicsControlRecordPair.Value;

				FString ParentComponentName = Record.ParentMeshComponent.IsValid() ?
					Record.ParentMeshComponent->GetName() : TEXT("NoParent");
				FString ChildComponentName = Record.ChildMeshComponent.IsValid() ?
					Record.ChildMeshComponent->GetName() : TEXT("NoChild");

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
					Record.PhysicsControl.IsEnabled() ? FColor::Green : FColor::Red, Text);
			}
		}
	}

	// Summary of control list
	if (bShowDebugControlList)
	{
		FString AllNames;

		for (const TPair<FName, FPhysicsControlRecord>& PhysicsControlRecordPair :
			ControlRecords)
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
			FString::Printf(TEXT("%d Controls: %s"), ControlRecords.Num(), *AllNames));

	}

	// Detailed body modifiers - if there's a filter
	if (!DebugBodyModifierDetailFilter.IsEmpty())
	{
		for (const TPair<FName, FPhysicsBodyModifierRecord>& PhysicsBodyModifierPair :
			BodyModifierRecords)
		{
			const FName Name = PhysicsBodyModifierPair.Key;

			if (Name.ToString().Contains(DebugBodyModifierDetailFilter))
			{
				const FPhysicsBodyModifierRecord& Record = PhysicsBodyModifierPair.Value;

				FString ComponentName = Record.MeshComponent.IsValid() ? Record.MeshComponent->GetName() : TEXT("None");

				FString Text = FString::Printf(
					TEXT("%s: %s: %s %s GravityMultiplier %f BlendWeight %f"),
					*Name.ToString(),
					*ComponentName,
					*UEnum::GetValueAsString(Record.BodyModifier.ModifierData.MovementType),
					*UEnum::GetValueAsString(Record.BodyModifier.ModifierData.CollisionType),
					Record.BodyModifier.ModifierData.GravityMultiplier,
					Record.BodyModifier.ModifierData.PhysicsBlendWeight);

				GEngine->AddOnScreenDebugMessage(-1, 0.0f, 
					Record.BodyModifier.ModifierData.MovementType == EPhysicsMovementType::Simulated ?
					FColor::Green : FColor::Red, Text);
			}
		}
	}

	// Summary of control list
	if (bShowDebugBodyModifierList)
	{
		FString AllNames;

		for (const TPair<FName, FPhysicsBodyModifierRecord>& PhysicsBodyModifierPair : BodyModifierRecords)
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
			FString::Printf(TEXT("%d Body modifiers: %s"), BodyModifierRecords.Num(), *AllNames));

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

	const FConstraintInstance* ConstraintInstance = Record.ConstraintInstance.Get();

	const bool bHaveLinear = Record.PhysicsControl.ControlData.LinearStrength > 0;
	const bool bHaveAngular = Record.PhysicsControl.ControlData.AngularStrength > 0;

	if (Record.PhysicsControl.IsEnabled() && ConstraintInstance)
	{
		FBodyInstance* ChildBodyInstance = UE::PhysicsControl::GetBodyInstance(
			Record.ChildMeshComponent.Get(), Record.PhysicsControl.ChildBoneName);
		if (!ChildBodyInstance)
		{
			return;
		}
		FTransform ChildBodyTM = ChildBodyInstance->GetUnrealWorldTransform();

		FBodyInstance* ParentBodyInstance = UE::PhysicsControl::GetBodyInstance(
			Record.ParentMeshComponent.Get(), Record.PhysicsControl.ParentBoneName);
		const FTransform ParentBodyTM = ParentBodyInstance ? ParentBodyInstance->GetUnrealWorldTransform() : FTransform();

		FTransform TargetTM;
		FVector TargetVelocity;
		FVector TargetAngularVelocity;
		CalculateControlTargetData(TargetTM, TargetVelocity, TargetAngularVelocity, Record, true);

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

