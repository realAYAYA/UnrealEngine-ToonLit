// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlRecord.h"
#include "UObject/ObjectPtr.h"
#include "UObject/NameTypes.h"

class USkeletalMeshComponent;
class UPhysicsControlComponent;
struct FPhysicsControl;

/**
 * Internal/private data/implementation for UPhysicsControlComponent
 */
struct FPhysicsControlComponentImpl
{
public:

	FPhysicsControlComponentImpl(UPhysicsControlComponent* InOwner) : Owner(InOwner) {}

	/**
	 * Retrieves the bone data for the specified bone given the skeletal mesh component.
	 * 
	 * @param OutBoneData If successful, this will contain the output bone data.
	 * @param InSkeletalMeshComponent Required to be a valid pointer to a skeletal mesh component
	 * @param InBoneName The name of the bone to retrieve data for
	 * @return true if the bone and data were found, false if not (in which case warnings will be logged)
	 */
	bool GetBoneData(
		FCachedSkeletalMeshData::FBoneData& OutBoneData,
		const USkeletalMeshComponent*       InSkeletalMeshComponent,
		const FName                         InBoneName) const;

	/**
	 * Retrieves the control for the name. Note that if Name is blank then the first control will be returned,
	 * assuming there is one.
	 */
	FPhysicsControl* FindControl(const FName Name);
	FPhysicsControlRecord* FindControlRecord(const FName Name);

	/**
	 * Updates the world-space bone positions etc for each skeleton we're tracking
	 */
	void UpdateCachedSkeletalBoneData(float DeltaTime);

	/**
	 * @return true if the difference between the old and new TMs exceeds the teleport
	 * translation/rotation thresholds
	 */
	bool DetectTeleport(const FTransform& OldTM, const FTransform& NewTM) const;

	/**
	 * @return true if the difference between the old and new TMs exceeds the teleport
	 * translation/rotation thresholds
	 */
	bool DetectTeleport(
		const FVector& OldPosition, const FQuat& OldOrientation,
		const FVector& NewPosition, const FQuat& NewOrientation) const;

	/**
	 * Terminates the underlying physical constraints, resets our internal stored state for each control,
	 * and optionally deletes all record of the controls.
	 */
	void ResetControls(bool bKeepControlRecords);

	/**
	 * Starts caching skeletal mesh poses, and registers for a tick pre-requisite
	 */
	void AddSkeletalMeshReferenceForCaching(USkeletalMeshComponent* SkeletalMeshComponent);

	/**
	 * Stops caching skeletal mesh poses (if this is the last one), and deregisters for a tick pre-requisite.
	 * Returns true if this was the last reference, false otherwise.
	 */
	bool RemoveSkeletalMeshReferenceForCaching(USkeletalMeshComponent* SkeletalMeshComponent);

	/**
	 * Records that a modifier is working with the skeletal mesh and stores original data if necessary
	 */
	void AddSkeletalMeshReferenceForModifier(USkeletalMeshComponent* SkeletalMeshComponent);

	/**
	 * Records that a modifier has stopped working with the skeletal mesh and restores original data if necessary.
	 * Returns true if this was the last reference, false otherwise.
	 */
	bool RemoveSkeletalMeshReferenceForModifier(USkeletalMeshComponent* SkeletalMeshComponent);

	/** Update the constraint based on the record. */
	void ApplyControl(FPhysicsControlRecord& Record);

	/**
	 * Updates the constraint strengths. Returns true if there is some strength, 
	 * false otherwise (e.g. to skip setting targets)
	 */
	bool ApplyControlStrengths(FPhysicsControlRecord& Record, FConstraintInstance* ConstraintInstance);

	/**
	 * Calculates the Target TM and velocities that will get passed to the constraint - so this 
	 * is a target that is defined in the space of the parent body (or in world space, if it doesn't exist).
	 */
	void CalculateControlTargetData(
		FTransform&                  OutTargetTM, 
		FVector&                     OutTargetVelocity,
		FVector&                     OutTargetAngularVelocity,
		const FPhysicsControlRecord& Record,
		bool                         bCalculateVelocity) const;

	/**
	 * This will set the kinematic target for the appropriate body based on the weighted target position and 
	 * orientation (and whether any were found) for any controls that are related to the body modifier.
	 */
	void ApplyKinematicTarget(const FPhysicsBodyModifier& BodyModifier) const;

	/**
	 * Retrieves the body modifier for the name. Note that if Name is blank then the first modifier will be returned,
	 * assuming there is one.
	 */
	FPhysicsBodyModifier* FindBodyModifier(const FName Name);

	FName GetUniqueControlName(const FName ParentBoneName, const FName ChildBoneName) const;
	FName GetUniqueBodyModifierName(const FName BoneName) const;

public:

	// Cached transforms from each skeletal mesh we're working with. Will be updated at the
	// beginning of each tick.
	TMap<TObjectPtr<USkeletalMeshComponent>, FCachedSkeletalMeshData> CachedSkeletalMeshDatas;

	// Track which skeletons have been affected by a body modifier - some settings get overridden
	// and then need to be restored when the last body modifier is destroyed.
	TMap<TObjectPtr<USkeletalMeshComponent>, FModifiedSkeletalMeshData> ModifiedSkeletalMeshDatas;

	TMap<FName, FPhysicsControlRecord> PhysicsControlRecords;
	TMap<FName, FPhysicsBodyModifier> PhysicsBodyModifiers;

private:
	UPhysicsControlComponent* Owner;
};

