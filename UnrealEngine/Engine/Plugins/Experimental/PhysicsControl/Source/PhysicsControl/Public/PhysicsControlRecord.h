// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"

struct FConstraintInstance;
struct FBodyInstance;
class UMeshComponent;

/**
 * There will be a PhysicsControlRecord created at runtime for every Control that has been created
 */
struct FPhysicsControlRecord
{
	FPhysicsControlRecord(
		const FPhysicsControl&       InControl,
		const FPhysicsControlTarget& InControlTarget,
		UMeshComponent*              InParentMeshComponent,
		UMeshComponent*              InChildMeshComponent)
		: PhysicsControl(InControl)
		, ControlTarget(InControlTarget)
		, ParentMeshComponent(InParentMeshComponent)
		, ChildMeshComponent(InChildMeshComponent)
	{}

	/** Removes any constraint and resets the state */
	void ResetConstraint();

	/** Returns the control point, which may be custom or automatic (centre of mass) */
	FVector GetControlPoint() const;

	/**
	 * Creates the constraint if necessary and stores it. Then initializes the constraint with the bodies.
	 * Returns true/false on success/failure */
	bool InitConstraint(UObject* ConstraintDebugOwner, FName ControlName);

	/** Ensures the constraint frame matches the control point in the record. */
	void UpdateConstraintControlPoint();

	/** Sets the control point to the center of mass of the child mesh (or to zero if that fails). */
	void ResetControlPoint();

	/** The configuration data */
	FPhysicsControl PhysicsControl;

	/**
	 * The position/orientation etc targets for the controls. These are procedural/explicit control targets -
	 * skeletal meshes have the option to use skeletal animation as well, in which case these targets are 
	 * expressed as relative to that animation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlTarget ControlTarget;

	/**  The mesh that will be doing the driving. Blank/non-existent means it will happen in world space */
	TWeakObjectPtr<UMeshComponent> ParentMeshComponent;

	/** The mesh that the control will be driving. */
	TWeakObjectPtr<UMeshComponent> ChildMeshComponent;

	/** The underlying constraint used to implement the control. */
	TSharedPtr<FConstraintInstance> ConstraintInstance;
};

/**
 * There will be a PhysicsBodyModifier created at runtime for every BodyInstance involved in the component
 */
struct FPhysicsBodyModifierRecord
{
	FPhysicsBodyModifierRecord(
		TWeakObjectPtr<UMeshComponent>  InMeshComponent, 
		const FName&                    InBoneName, 
		FPhysicsControlModifierData     InBodyModifierData)
		: MeshComponent(InMeshComponent)
		, BodyModifier(InBoneName, InBodyModifierData)
		, KinematicTargetPosition(FVector::ZeroVector)
		, KinematicTargetOrientation(FQuat::Identity)
		, bResetToCachedTarget(false)
	{}

	/**  The mesh that will be modified. */
	TWeakObjectPtr<UMeshComponent> MeshComponent;

	// The core data
	FPhysicsBodyModifier BodyModifier;

	/** 
	 * The target position when kinematic. Note that this is applied on top of any animation 
	 * target if bUseSkeletalAnimation is set. 
	 */
	FVector KinematicTargetPosition = FVector::ZeroVector;

	/**
	 * The target orientation when kinematic. Note that this is applied on top of any animation
	 * target if bUseSkeletalAnimation is set.
	 */
	FQuat KinematicTargetOrientation = FQuat::Identity;

	/** 
	 * If true then the body will be set to the transform/velocity stored in any cached target (if that
	 * exists), and then this flag will be cleared.
	 */
	uint8 bResetToCachedTarget:1;
};

/**
 * Used internally/only at runtime to track when a SkeletalMeshComponent is being controlled through
 * a modifier, and to restore settings when that stops.
 */
struct FModifiedSkeletalMeshData
{
public: 
	FModifiedSkeletalMeshData() : ReferenceCount(0) {}

public:

	/** The original setting for restoration when we're deleted */
	uint8 bOriginalUpdateMeshWhenKinematic : 1;

	/** The original setting for restoration when we're deleted */
	EKinematicBonesUpdateToPhysics::Type OriginalKinematicBonesUpdateType;

	/**
	 * Track when skeletal meshes are going to be used so this entry can be removed
	 */
	int32 ReferenceCount;
};

/**
 * Used internally/only at runtime to cache skeletal transforms at the start of the tick, to avoid
 * calculating them separately for every control.
 */
struct FCachedSkeletalMeshData
{
public:
	FCachedSkeletalMeshData() : ReferenceCount(0) {}

public:
	struct FBoneData
	{
		FBoneData()
			: Position(FVector::ZeroVector), Orientation(FQuat::Identity)
			, Velocity(FVector::ZeroVector), AngularVelocity(FVector::ZeroVector) {}
		FBoneData(const FVector& InPosition, const FQuat& InOrientation)
			: Position(InPosition), Orientation(InOrientation),
			Velocity(FVector::ZeroVector), AngularVelocity(FVector::ZeroVector) {}

		/**
		 * Sets position/orientation and calculates velocity/angular velocity - requires Dt > 0 
		 */
		void Update(const FVector& InPosition, const FQuat& InOrientation, float Dt);

		/**
		 * Sets position/orientation, and sets velocity to zero
		 */
		void Update(const FVector& InPosition, const FQuat& InOrientation);

		FTransform GetTM() const { return FTransform(Orientation, Position); }

		FVector Position;
		FQuat   Orientation;
		FVector Velocity;
		FVector AngularVelocity;
	};

public:

	/**
	 * The cached skeletal data, updated at the start of each tick
	 */
	TArray<FBoneData> BoneData;

	/**
	 * The component transform. This is only stored so we can detect teleports
	 */
	FTransform ComponentTM;

	/**
	 * Track when skeletal meshes are going to be used so this entry can be removed, and also so we
	 * can add a tick dependency
	 */
	int32 ReferenceCount;
};

//======================================================================================================================
inline void FCachedSkeletalMeshData::FBoneData::Update(const FVector& InPosition, const FQuat& InOrientation, float Dt)
{
	check(Dt > 0);

	Velocity = (InPosition - Position) / Dt;
	Orientation.EnforceShortestArcWith(InOrientation);
	// Note that quats multiply in the opposite order to TMs
	const FQuat DeltaQ = InOrientation * Orientation.Inverse();
	AngularVelocity = DeltaQ.ToRotationVector() / Dt;
	Position = InPosition;
	Orientation = InOrientation;
}

//======================================================================================================================
inline void FCachedSkeletalMeshData::FBoneData::Update(const FVector& InPosition, const FQuat& InOrientation)
{
	Position = InPosition;
	Orientation = InOrientation;
	Velocity = FVector::ZeroVector;
	AngularVelocity = FVector::ZeroVector;
}
