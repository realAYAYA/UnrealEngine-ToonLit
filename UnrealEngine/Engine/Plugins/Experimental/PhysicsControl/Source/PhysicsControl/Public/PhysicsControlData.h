// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "PhysicsControlData.generated.h"

class UMeshComponent;

/**
 * Used by Body Modifiers to specify how the physical bodies should move.  
 */
UENUM(BlueprintType)
enum class EPhysicsMovementType : uint8
{
	// Static means that the object won't be simulated, and it won't be moved according to the
	// kinematic target set in the Body Modifier (though something else might move it)
	Static,
	// Kinematic means that the object won't be simulated, but will be moved according to the
	// kinematic target set in the Body Modifier.
	Kinematic,
	// Simulated means that the object will be controlled by the physics solver
	Simulated
};

/**
 * Contains data associated with how physical bodies should be controlled/directed towards their targets. 
 * The underlying control is done through damped springs, so the parameters here relate to that.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlData
{
	GENERATED_BODY()

	FPhysicsControlData()
		: LinearStrength(1.0f)
		, LinearDampingRatio(1.0f)
		, LinearExtraDamping(0.0f)
		, MaxForce(0.0f)
		, AngularStrength(1.0f)
		, AngularDampingRatio(1.0f)
		, AngularExtraDamping(0.0f)
		, MaxTorque(0.0f)
	{
	}

	/** The strength used to drive linear motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float LinearStrength;

	/** 
	 * The amount of damping associated with the linear strength. A value of 1 Results in critically 
	 * damped motion where the control drives as quickly as possible to the target without overshooting. 
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float LinearDampingRatio;

	/** 
	 * The amount of additional linear damping. This is added to the damping that comes from LinearDampingRatio
	 * and can be useful when you want damping even when LinearStrength is zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float LinearExtraDamping;

	/** The maximum force used to drive the linear motion. Zero indicates no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float MaxForce;

	/** The strength used to drive angular motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularStrength;

	/** 
	 * The amount of damping associated with the angular strength. A value of 1 Results in critically 
	 * damped motion where the control drives as quickly as possible to the target without overshooting. 
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularDampingRatio;

	/** 
	 * The amount of additional angular damping. This is added to the damping that comes from AngularDampingRatio
	 * and can be useful when you want damping even when AngularStrength is zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularExtraDamping;

	/** The maximum torque used to drive the angular motion. Zero indicates no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float MaxTorque;
};

/**
 * These parameters allow modification of the parameters in FPhysicsControlData for two reasons:
 * 1. They allow per-axis settings for the linear components (e.g. so you can drive an object 
 *    horizontally but still let it fall under gravity)
 * 2. They make it easy to create the controls with "default" strength/damping (e.g. taken from the
 *    physics asset) in FPhysicsControlData, and then the strength/damping etc can be scaled every 
 *    tick (typically between 0 and 1, though that is up to the user).
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlMultipliers
{
	GENERATED_BODY()

	FPhysicsControlMultipliers()
		: LinearStrengthMultiplier(1.0)
		, LinearExtraDampingMultiplier(1.0)
		, MaxForceMultiplier(1.0)
		, AngularStrengthMultiplier(1.0)
		, AngularExtraDampingMultiplier(1.0)
		, MaxTorqueMultiplier(1.0)
	{
	}

	// Per-direction multiplier on the linear strength.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	FVector LinearStrengthMultiplier;

	// Per-direction multiplier on the linear extra damping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	FVector LinearExtraDampingMultiplier;

	// Per-direction multiplier on the maximum force that can be applied. Note that zero means zero force.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	FVector MaxForceMultiplier;

	// Multiplier on the angular strength.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularStrengthMultiplier;

	// Multiplier on the angular extra damping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularExtraDampingMultiplier;

	// Per-direction multiplier on the maximum torque that can be applied. Note that zero means zero torque.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float MaxTorqueMultiplier;
};

/**
 * Defines a target position and orientation, and also the target velocity and angular velocity.
 * In many cases the velocities will be calculated automatically (e.g. when setting the target position,
 * the component will optionally calculate an implied velocity. However, the user can also specify a 
 * target velocity directly. Note that the velocity influences the control through the damping parameters
 * in FPhysicsControlData
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlTarget
{
	GENERATED_BODY()

	FPhysicsControlTarget()
		: TargetPosition(ForceInitToZero)
		, TargetVelocity(ForceInitToZero)
		, TargetOrientation(ForceInitToZero)
		, TargetAngularVelocity(ForceInitToZero)
		, bApplyControlPointToTarget(false)
	{
	}

	/** The target position of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FVector TargetPosition;

	/** The target velocity of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FVector TargetVelocity;

	/** The target orientation of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FRotator TargetOrientation;

	/** The target angular velocity (revolutions per second) of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FVector TargetAngularVelocity;

	/** 
	 * Whether to use the ControlPoint as an offset for the target transform, as well as the 
	 * physical body. If true then the target TM is treated as a target transform for the actual 
	 * object, though the control is still applied through the control point (which is at the 
	 * center of mass by default). If false then it is treated as a target transform for the 
	 * control point on the object.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	bool bApplyControlPointToTarget;
};

/**
 * General settings for a control
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlSettings
{
	GENERATED_BODY()

	FPhysicsControlSettings()
		: ControlPoint(ForceInitToZero)
		, bUseSkeletalAnimation(true)
		, SkeletalAnimationVelocityMultiplier(1.0f)
		, bDisableCollision(false)
		, bAutoDisable(false)
	{
	}

	/**
	 * The position of the control point relative to the child mesh. Note that this can't be authored
	 * directly here/on creation - it needs to be set after creation in UPhysicsControlComponent::SetControlPoint
	 */
	UPROPERTY()
	FVector ControlPoint;

	/** If true then the target will be applied on top of the skeletal animation (if there is any) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	bool bUseSkeletalAnimation;

	/** The amount of skeletal animation velocity to use in the targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	float SkeletalAnimationVelocityMultiplier;

	/**
	 * Whether or not this control should disable collision between the parent and child bodies (only
	 * has an effect if there is a parent body)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	bool bDisableCollision;

	/**
	 * Whether or not this control should automatically disable itself at the end of each tick. This
	 * can be useful when it is more convenient to have a branch that handles some condition (e.g. character 
	 * is flailing) by updating/enabling the control, and to then have the control automatically get 
	 * disabled when the branch is no longer taken.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	bool bAutoDisable;
};

/**
 * Structure that determines a "control" - this contains all the information needed to drive (with spring-dampers)
 * a child body relative to a parent body. These bodies will be associated with either a static or skeletal mesh.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControl
{
	GENERATED_BODY()

	FPhysicsControl() {}

	FPhysicsControl(
		UMeshComponent*              InParentMeshComponent,
		const FName&                 InParentBoneName,
		UMeshComponent*              InChildMeshComponent,
		const FName&                 InChildBoneName,
		const FPhysicsControlData&   InControlData,
		const FPhysicsControlTarget& InControlTarget,
		const FPhysicsControlSettings& InControlSettings)
		: ParentMeshComponent(InParentMeshComponent)
		, ParentBoneName(InParentBoneName)
		, ChildMeshComponent(InChildMeshComponent)
		, ChildBoneName(InChildBoneName)
		, ControlData(InControlData)
		, ControlTarget(InControlTarget)
		, ControlSettings(InControlSettings)
	{
	}

	/**  The mesh that will be doing the driving. Blank/non-existent means it will happen in world space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TObjectPtr<UMeshComponent> ParentMeshComponent;

	/** The name of the skeletal mesh bone or the name of the static mesh body that will be doing the driving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName ParentBoneName;

	/** The mesh that the control will be driving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TObjectPtr<UMeshComponent> ChildMeshComponent;

	/** 
	 * The name of the skeletal mesh bone or the name of the static mesh body that the control 
	 * will be driving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName ChildBoneName;

	/** 
	 * Strength and damping parameters. Can be modified at any time, but will sometimes have 
	 * been set once during initialization 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlData ControlData;

	/**
	 * Multipliers for the ControlData. These will typically be modified dynamically, and also expose the ability
	 * to set directional strengths
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlMultipliers ControlMultipliers;

	/**
	 * The position/orientation etc targets for the controls. These are procedural/explicit control targets -
	 * skeletal meshes have the option to use skeletal animation as well, in which case these targets are 
	 * expressed as relative to that animation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlTarget ControlTarget;

	/**
	 * More general settings for the control
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlSettings ControlSettings;
};

