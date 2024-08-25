// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PhysicsControlLog.h"

class UPhysicsAsset;

#include "RigidBodyControlData.h"
#include "PhysicsControlRecord.h"

struct FAnimNode_RigidBodyWithControl;
struct FPhysicsControlLimbBones;
struct FPhysicsControlLimbSetupData;
struct FPhysicsControlSetUpdates;
struct FReferenceSkeleton;
struct FPhysicsControlNameRecords;

namespace UE
{
namespace PhysicsControl
{

constexpr int32 MaxNumControlsOrModifiersPerName = 16;

// Parse the skeleton tree to figure out which bones are associated with which limbs. 
PHYSICSCONTROL_API TMap<FName, FPhysicsControlLimbBones> GetLimbBones(
	const TArray<FPhysicsControlLimbSetupData>& LimbSetupData, 
	const FReferenceSkeleton&                   RefSkeleton, 
	UPhysicsAsset*                              PhysicsAsset);

// Populates the supplied body modifier names, control names and name records structures with the
// names and sets that could be created for the supplied node, limb bones, skeleton and physics asset.
PHYSICSCONTROL_API void CollectOperatorNames(
	const FAnimNode_RigidBodyWithControl*            Node, 
	const FPhysicsControlCharacterSetupData&         CharacterSetupData,
	const FPhysicsControlAndBodyModifierCreationDatas& AdditionalControlsAndBodyModifiers,
	TMap<FName, FPhysicsControlLimbBones>            AllLimbBones,
	const FReferenceSkeleton&                        RefSkeleton, 
	UPhysicsAsset*                                   PhysicsAsset, 
	TSet<FName>&                                     BodyModifierNames, 
	TSet<FName>&                                     ControlNames, 
	FPhysicsControlNameRecords&                      NameRecords);

// Creates the body modifiers, controls and sets for the supplied node, limb bones, skeleton and physics asset.
PHYSICSCONTROL_API void CreateOperatorsForNode(
	FAnimNode_RigidBodyWithControl*                  Node, 
	const FPhysicsControlCharacterSetupData&         CharacterSetupData,
	const FPhysicsControlAndBodyModifierCreationDatas& AdditionalControlsAndBodyModifiers,
	TMap<FName, FPhysicsControlLimbBones>            AllLimbBones,
	const FReferenceSkeleton&                        RefSkeleton, 
	UPhysicsAsset*                                   PhysicsAsset, 
	FPhysicsControlNameRecords&                      NameRecords);

// Adds the specified additional sets to the supplied Name Records structure.
PHYSICSCONTROL_API void CreateAdditionalSets(
	const FPhysicsControlSetUpdates& AdditionalSets, 
	const TSet<FName>&               BodyModifierNames, 
	const TSet<FName>&               ControlNames, 
	FPhysicsControlNameRecords&      NameRecords);

// Adds the specified additional sets to the supplied Name Records structure.
PHYSICSCONTROL_API void CreateAdditionalSets(
	const FPhysicsControlSetUpdates&             AdditionalSets, 
	const TMap<FName, FRigidBodyModifierRecord>& BodyModifierRecords, 
	const TMap<FName, FRigidBodyControlRecord>&  Controls, 
	FPhysicsControlNameRecords&                  NameRecords);

// Adds the specified additional sets to the supplied Name Records structure.
PHYSICSCONTROL_API void CreateAdditionalSets(
	const FPhysicsControlSetUpdates&             AdditionalSets, 
	const TMap<FName, FPhysicsBodyModifierRecord>&     BodyModifierRecords,
	const TMap<FName, FPhysicsControlRecord>&    Controls,
	FPhysicsControlNameRecords&                  NameRecords);

//======================================================================================================================
template<typename CollectionType>
FName GetUniqueName(const FString& NameBase, const CollectionType& ExistingNames, const int32 MaxNameIndex)
{
	FString NameStr = NameBase;

	// If the number gets too large, almost certainly we're in some nasty situation where this is
	// getting called in a loop. Better to quit and fail, rather than allow the constraint set to
	// increase without bound. 
	for (int32 Index = 0; Index < MaxNameIndex; ++Index)
	{
		const FName Name(NameStr);
		if (!ExistingNames.Find(Name))
		{
			return Name;
		}
		NameStr = FString::Format(TEXT("{0}_{1}"), { NameBase, Index });
	}
	UE_LOG(LogPhysicsControl, Warning,
		TEXT("Unable to find a suitable Control name - the limit of (%d) has been exceeded"),
		MaxNameIndex);

	return NAME_None;
}

//======================================================================================================================
template<typename CollectionType>
FName GetUniqueBodyModifierName(const FName BodyName, const CollectionType& ExistingNames, const FString& NamePrefix)
{
	FString NameBase = NamePrefix;
	if (!BodyName.IsNone())
	{
		NameBase += BodyName.ToString();
	}
	else
	{
		NameBase = TEXT("Body");
	}

	return GetUniqueName(NameBase, ExistingNames, MaxNumControlsOrModifiersPerName);
}

//======================================================================================================================
template<typename CollectionType>
FName GetUniqueControlName(
	const FName ParentBodyName, const FName ChildBodyName, const CollectionType& ExistingNames, const FString& NamePrefix)
{
	FString NameBase = NamePrefix;
	if (!ParentBodyName.IsNone())
	{
		NameBase += ParentBodyName.ToString() + TEXT("_");
	}
	if (!ChildBodyName.IsNone())
	{
		NameBase += ChildBodyName.ToString();
	}

	return GetUniqueName(NameBase, ExistingNames, MaxNumControlsOrModifiersPerName);
}






} // namespace PhysicsControlComponent
} // namespace UE
