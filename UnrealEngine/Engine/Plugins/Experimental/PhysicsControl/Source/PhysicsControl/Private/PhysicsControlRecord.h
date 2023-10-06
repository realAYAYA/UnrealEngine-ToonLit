// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/EngineTypes.h"

struct FConstraintInstance;
struct FBodyInstance;
class UMeshComponent;

/**
 * The basic state of a physics control, created for every control record.
 */
struct FPhysicsControlState
{
	/** Removes any constraint and resets the state */
	void Reset();

	TSharedPtr<FConstraintInstance> ConstraintInstance;

	bool bEnabled = false;
};

/**
 * There will be a PhysicsControlRecord created at runtime for every Control that has been created
 */
struct FPhysicsControlRecord
{
	FPhysicsControlRecord(const FPhysicsControl& InControl) : PhysicsControl(InControl)
	{
	}

	/** 
	 * Creates the constraint (if necessary) and stores it in the state. ConstraintDebugOwner is passed 
	 * to the constraint on creation. 
	 */
	FConstraintInstance* CreateConstraint(UObject* ConstraintDebugOwner, FName ControlName);

	/** Ensures the constraint frame matches the control point in the record. */
	void UpdateConstraintControlPoint();

	/** Sets the control point to the center of mass of the child mesh (or to zero if that fails). */
	void ResetControlPoint();

	/** The configuration data */
	FPhysicsControl PhysicsControl;

	/** The instance/runtime state - instantiated and kept up to date (during the tick) with PhysicsControl. */
	FPhysicsControlState PhysicsControlState;
};

/**
 * There will be a PhysicsBodyModifier created at runtime for every BodyInstance involved in the component
 */
struct FPhysicsBodyModifier
{
	FPhysicsBodyModifier(
		TObjectPtr<UMeshComponent> InMeshComponent, 
		const FName&               InBoneName, 
		EPhysicsMovementType       InMovementType,
		ECollisionEnabled::Type    InCollisionType,
		float                      InGravityMultiplier,
		float                      InPhysicsBlendWeight,
		bool                       InUseSkeletalAnimation,
		bool                       InUpdateKinematicFromSimulation)
		: MeshComponent(InMeshComponent)
		, BoneName(InBoneName)
		, MovementType(InMovementType)
		, CollisionType(InCollisionType)
		, GravityMultiplier(InGravityMultiplier)
		, PhysicsBlendWeight(InPhysicsBlendWeight)
		, KinematicTargetPosition(FVector::ZeroVector)
		, KinematicTargetOrientation(FQuat::Identity)
		, bUseSkeletalAnimation(InUseSkeletalAnimation)
		, bUpdateKinematicFromSimulation(InUpdateKinematicFromSimulation)
		, bResetToCachedTarget(false)
	{}

	/**  The mesh that will be modified. */
	TObjectPtr<UMeshComponent> MeshComponent;

	/** The name of the skeletal mesh bone or the name of the static mesh body that will be modified. */
	FName BoneName;

	/** How the associated body should move. */
	EPhysicsMovementType MovementType = EPhysicsMovementType::Kinematic;

	/** How the associated body should collide/interact */
	ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics;

	/**
	 * Multiplier for gravity applied to the body. Note that if the body itself has gravity disabled, then
	 * setting this to 1 will not enable gravity.
	 */
	float GravityMultiplier = 1.0f;

	/**
	 * Blend weight (between 0 and 1) that is used to set/override the one in the body instance
	 */
	float PhysicsBlendWeight = 1.0f;

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

	/** If true then the target will be applied on top of the skeletal animation (if there is any) */
	uint8 bUseSkeletalAnimation:1;

	/** 
	 * If true then the associated actor's transform will be updated from the simulation when it is 
	 * kinematic. This is most likely useful when using async physics in order to prevent different 
	 * parts of the skeleton from being torn apart. 
	 */
	uint8 bUpdateKinematicFromSimulation:1;

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
