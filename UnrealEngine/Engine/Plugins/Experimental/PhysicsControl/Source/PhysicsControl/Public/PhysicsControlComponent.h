// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlData.h"
#include "PhysicsControlLimbData.h"

#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "Templates/PimplPtr.h"

#include "PhysicsControlComponent.generated.h"

class FPrimitiveDrawInterface;
struct FPhysicsControlComponentImpl;
struct FPhysicsControlRecord;
struct FPhysicsBodyModifier;
struct FConstraintInstance;

/**
 * Specifies the type of control that is created when making controls from a skeleton or a set of limbs. 
 * Note that if controls are made individually then other options are available - i.e. in a character, 
 * any body part can be controlled relative to any other part, or indeed any other object.
 */
UENUM(BlueprintType)
enum class EPhysicsControlType : uint8
{
	/** Control is done in world space, so each object/part is driven independently */
	WorldSpace,
	/** Control is done in the space of the parent of each object */
	ParentSpace,
};

/**
 * This is the main Physics Control Component class which manages Controls and Body Modifiers associated 
 * with one or more static or skeletal meshes. You can add this as a component to an actor containing a 
 * mesh and then use it to create, configure and destroy Controls/Body Modifiers:
 * 
 * Controls are used to control one physics body relative to another (or the world). These controls are done
 * through physical spring/damper drives.
 * 
 * Body Modifiers are used to update the most important physical properties of physics bodies such as whether 
 * they are simulated vs kinematic, or whether they experience gravity.
 * 
 * Note that Controls and Body Modifiers are given names (which are predictable). These names can then be stored 
 * (perhaps in arrays) to make it easy to quickly change multiple Controls/Body Modifiers.
 */
UCLASS(meta = (BlueprintSpawnableComponent), ClassGroup = Physics, Experimental)
class PHYSICSCONTROL_API UPhysicsControlComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Makes a new control for mesh components
	 * 
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlTarget Describes the initial target for the new control
	 * @param ControlSettings General settings for the control
	 * @param bEnabled      If true then the control will be enabled immediately. If false you will need to call
	 *                      SetControlEnabled(true) in order to activate it.
	 * @return The name of the new control
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FName MakeControl(
		UMeshComponent*         ParentMeshComponent,
		FName                   ParentBoneName,
		UMeshComponent*         ChildMeshComponent,
		FName                   ChildBoneName,
		FPhysicsControlData     ControlData,
		FPhysicsControlTarget   ControlTarget, 
		FPhysicsControlSettings ControlSettings,
		bool                    bEnabled = true);

	/**
	 * Makes a new control for mesh components
	 * 
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlTarget Describes the initial target for the new control
	 * @param ControlSettings General settings for the control
	 * @param bEnabled      If true then the control will be enabled immediately. If false you will need to call
	 *                      SetControlEnabled(true) in order to enable it.
	 * @return True if a new control was created, false if a control of the specified name already exists
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool MakeNamedControl(
		FName                   Name,
		UMeshComponent*         ParentMeshComponent,
		FName                   ParentBoneName,
		UMeshComponent*         ChildMeshComponent,
		FName                   ChildBoneName,
		FPhysicsControlData     ControlData, 
		FPhysicsControlTarget   ControlTarget, 
		FPhysicsControlSettings ControlSettings, 
		bool                    bEnabled = true);

	/**
	 * Makes a collection of controls controlling a skeletal mesh
	 * 
	 * @param SkeletalMeshComponent The skeletal mesh which will have controls
	 * @param BoneName The name of the bone below which controls should be created. Each bone will be the child in a control
	 * @param bIncludeSelf Whether or not to include BoneName when creating controls
	 * @param ControlType What type of control to create. This determines what the parent will be for each control
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlSettings General settings for the control
	 * @param bEnabled      If true then the control will be enabled immediately. If false you will need to call
	 *                      SetControlEnabled(true) in order to enable it.
	 * @return An array of the controls that have been created
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> MakeControlsFromSkeletalMeshBelow(
		USkeletalMeshComponent* SkeletalMeshComponent,
		FName                   BoneName,
		bool                    bIncludeSelf,
		EPhysicsControlType     ControlType,
		FPhysicsControlData     ControlData,
		FPhysicsControlSettings ControlSettings,
		bool                    bEnabled = true);

	/**
	 * Makes a collection of ParentSpace controls controlling a skeletal mesh, initializing
	 * them with a constraint profile
	 * 
	 * @param SkeletalMeshComponent The skeletal mesh which will have controls
	 * @param BoneName The name of the bone below which controls should be created. Each bone will be the child in a control
	 * @param bIncludeSelf Whether or not to include BoneName when creating controls
	 * @param ConstraintProfile The constraint profile to use for initializing the control strength and
	 *                          damping (etc) parameters. Note that the controls will all be created in "
	 *                          parent space" - i.e. with each part controlled relative to its parent. The
	 *                          strength and damping will be taken from the values that the relevant joint
	 *                          in the physics asset would have given the constraint profile (or the default
	 *                          profile if it can't be found) - though they will not match exactly if the linear
	 *                          drive and different x/y/z values, or if the angular drive was using twist/swing
	 *                          instead of slerp. Note also that the joint constraints do not use the animation
	 *                          velocity as a target, so when creating controls in this way the control settings
	 *                          will set the skeletal animation velocity multiplier to zero.
	 * @param bEnabled      If true then the control will be enabled immediately. If false you will need to call
	 *                      SetControlEnabled(true) in order to enable it.
	 * @return An array of the controls that have been created
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> MakeControlsFromSkeletalMeshAndConstraintProfileBelow(
		USkeletalMeshComponent* SkeletalMeshComponent,
		FName                   BoneName,
		bool                    bIncludeSelf,
		FName                   ConstraintProfile,
		bool                    bEnabled = true);

	/**
	 * Makes a collection of controls controlling a skeletal mesh
	 *
	 * @param SkeletalMeshComponent The skeletal mesh which will have controls
	 * @param BoneNames The names of bones for which controls should be created. Each bone will be the child in a control
	 * @param ControlType What type of control to create. This determines what the parent will be for each control
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlSettings General settings for the control
	 * @param bEnabled      If true then the control will be enabled immediately. If false you will need to call
	 *                      SetControlEnabled(true) in order to enable it.
	 * @return An array of the controls that have been created
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> MakeControlsFromSkeletalMesh(
		USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FName>&    BoneNames,
		EPhysicsControlType     ControlType,
		FPhysicsControlData     ControlData,
		FPhysicsControlSettings ControlSettings,
		bool                    bEnabled = true);

	/**
	 * Makes a collection of ParentSpace controls controlling a skeletal mesh, initializing them 
	 * with a constraint profile
	 *
	 * @param SkeletalMeshComponent The skeletal mesh which will have controls
	 * @param BoneNames The names of bones for which controls should be created. Each bone will be the child in a control
	 * @param ConstraintProfile The constraint profile to use for initializing the control strength and
	 *                          damping (etc) parameters. Note that the controls will all be created in "
	 *                          parent space" - i.e. with each part controlled relative to its parent. The
	 *                          strength and damping will be taken from the values that the relevant joint
	 *                          in the physics asset would have given the constraint profile (or the default
	 *                          profile if it can't be found) - though they will not match exactly if the linear
	 *                          drive and different x/y/z values, or if the angular drive was using twist/swing
	 *                          instead of slerp. Note also that the joint constraints do not use the animation
	 *                          velocity as a target, so when creating controls in this way the control settings
	 *                          will set the skeletal animation velocity multiplier to zero.
	 * @param bEnabled      If true then the control will be enabled immediately. If false you will need to call
	 *                      SetControlEnabled(true) in order to enable it.
	 * @return An array of the controls that have been created
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> MakeControlsFromSkeletalMeshAndConstraintProfile(
		USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FName>&    BoneNames,
		FName                   ConstraintProfile,
		bool                    bEnabled = true);

	/**
	 * Calculates which bones belong to which limb in a skeletal mesh
	 * 
	 * @param SkeletalMeshComponent The skeletal mesh which will be analyzed
	 * @param LimbSetupData This needs to be filled in with the list of limbs to "discover". Note that the 
	 *                      limbs should be listed starting at the "leaf" (i.e. outer) parts of the skeleton 
	 *                      first, typically finishing with the spine. In addition, the spine limb is typically 
	 *                      specified using the first spine bone, but flagging it to include its parent 
	 *                      (normally the pelvis). 
	 * @return A map of limb names to bones
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TMap<FName, FPhysicsControlLimbBones> GetLimbBonesFromSkeletalMesh(
		USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FPhysicsControlLimbSetupData>& LimbSetupData) const;

	/**
	 * Makes a collection of controls controlling a skeletal mesh, grouped together in limbs
	 *
	 * @param AllControls A single container for all the controls that have been created
	 * @param LimbBones A map relating the limbs and the bones that they contain. Typically create this 
	 *                  using GetLimbBonesFromSkeletalMesh
	 * @param ControlType What type of control to create. This determines what the parent will be for each control
	 * @param ControlData Describes the initial strength etc of the new control
	 * @param ControlSettings General settings for the control
	 * @param bEnabled If true then the control will be enabled immediately. If false you will need to call
	 *                 SetControlEnabled(true) in order to enable it.
	 * @return A map containing the controls for each limb
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TMap<FName, FPhysicsControlNameArray> MakeControlsFromLimbBones(
		FPhysicsControlNameArray&                    AllControls,
		const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
		EPhysicsControlType                          ControlType,
		FPhysicsControlData                          ControlData,
		FPhysicsControlSettings                      ControlSettings,
		bool                                         bEnabled = true);

	/**
	 * Makes a collection of ParentSpace controls controlling a skeletal mesh, grouped together in limbs, initializing
	 * them with a constraint profile
	 *
	 * @param AllControls A single container for all the controls that have been created
	 * @param LimbBones A map relating the limbs and the bones that they contain. Typically create this
	 *                  using GetLimbBonesFromSkeletalMesh
	 * @param ConstraintProfile The constraint profile to use for initializing the control strength and 
	 *                          damping (etc) parameters. Note that the controls will all be created in "
	 *                          parent space" - i.e. with each part controlled relative to its parent. The 
	 *                          strength and damping will be taken from the values that the relevant joint 
	 *                          in the physics asset would have given the constraint profile (or the default 
	 *                          profile if it can't be found) - though they will not match exactly if the linear
	 *                          drive and different x/y/z values, or if the angular drive was using twist/swing
	 *                          instead of slerp. Note also that the joint constraints do not use the animation 
	 *                          velocity as a target, so when creating controls in this way the control settings
	 *                          will set the skeletal animation velocity multiplier to zero.
	 * @param bEnabled If true then the control will be enabled immediately. If false you will need to call
	 *                 SetControlEnabled(true) in order to enable it.
	 * @return A map containing the controls for each limb
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TMap<FName, FPhysicsControlNameArray> MakeControlsFromLimbBonesAndConstraintProfile(
		FPhysicsControlNameArray&                    AllControls,
		const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
		FName                                        ConstraintProfile,
		bool                                         bEnabled = true);

	/**
	 * Destroys a control
	 *
	 * @param Name The name of the control to destroy. 
	 * @return     Returns true if the control was found and destroyed, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool DestroyControl(FName Name);

	/**
	 * Destroys all controls
	 *
	 * @param Names The names of the controls to destroy. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void DestroyAllControls(const TArray<FName>& Names);

	/**
	 * Modifies an existing control data - i.e. the strengths etc of the control driving towards the target
	 *
	 * @param Name The name of the control to modify. 
	 * @param ControlData The new control data
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlData(FName Name, FPhysicsControlData ControlData, bool bEnableControl = true);

	/**
	 * Modifies existing control data - i.e. the strengths etc of the controls driving towards the targets
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param ControlData The new control data
	 * @param bEnableControl Enables the control if it is currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllControlDatas(const TArray<FName>& Names, FPhysicsControlData ControlData, bool bEnableControl = true);

	/**
	 * Modifies an existing control data using the multipliers
	 *
	 * @param Name The name of the control to modify. 
	 * @param ControlMultipliers The new control multipliers
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlMultipliers(FName Name, FPhysicsControlMultipliers ControlMultipliers, bool bEnableControl = true);

	/**
	 * Modifies existing control data using the multipliers
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param ControlMultipliers The new control multipliers
	 * @param bEnableControl Enables the control if it is currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllControlMultipliers(
		const TArray<FName>&       Names, 
		FPhysicsControlMultipliers ControlMultipliers, 
		bool                       bEnableControl = true);

	/**
	 * Modifies an existing control's linear data - i.e. the strengths etc of the control driving towards the target
	 *
	 * @param Name The name of the control to modify. 
	 * @param Strength The strength used to drive linear motion
	 * @param DampingRatio The amount of damping associated with the linear strength. 1 Results in critically damped motion
	 * @param ExtraDamping The amount of additional linear damping
	 * @param MaxForce The maximum force used to drive the linear motion. Zero indicates no limit.
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlLinearData(
		FName Name,
		float Strength = 1.0f, 
		float DampingRatio = 1.0f, 
		float ExtraDamping = 0.0f, 
		float MaxForce = 0.0f, 
		bool  bEnableControl = true);

	/**
	 * Modifies an existing control's angular data - i.e. the strengths etc of the control driving towards the target
	 *
	 * @param Name The name of the control to modify. 
	 * @param Strength The strength used to drive angular motion
	 * @param DampingRatio The amount of damping associated with the angular strength. 1 Results in critically damped motion
	 * @param ExtraDamping The amount of additional angular damping
	 * @param MaxTorque The maximum torque used to drive the angular motion. Zero indicates no limit.
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlAngularData(
		FName Name,
		float Strength = 1.0f, 
		float DampingRatio = 1.0f, 
		float ExtraDamping = 0.0f, 
		float MaxTorque = 0.0f, 
		bool  bEnableControl = true);

	/**
	 * Sets the point at which controls will "push" the child object.
	 * 
	 * @param Name The name of the control to modify. 
	 * @param Position The position of the control point on the child mesh object (only relevant if that 
	 *        object is in use and is being simulated)
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlPoint(FName Name, const FVector Position);

	/**
	 * Resets the control point to the center of mass of the mesh
	 *
	 * @param Name The name of the control to modify. 
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool ResetControlPoint(FName Name);

	/**
	 * Modifies an existing control target - i.e. what it is driving towards, relative to the parent object
	 *
	 * @param Name The name of the control to modify. 
	 * @param ControlTarget The new target for the control
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTarget(FName Name, FPhysicsControlTarget ControlTarget, bool bEnableControl = true);

	/**
	 * Modifies existing control targets - i.e. what they are driving towards, relative to the parent objects
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param ControlTarget The new target for the controls
	 * @param bEnableControl Enables the control if it is currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllControlTargets(
		const TArray<FName>&  Names, 
		FPhysicsControlTarget ControlTarget, 
		bool                  bEnableControl = true);


	/**
	 * Modifies an existing control target - i.e. what it is driving towards, relative to the parent object
	 *
	 * @param Name The name of the control to modify. 
	 * @param Position The new position target for the control
	 * @param Orientation The new orientation target for the control
	 * @param VelocityDeltaTime If non-zero, the target velocity will be calculated using the current target 
	 *        position. If zero, the target velocity will be set to zero.    
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @param bApplyControlPointToTarget If true, then the target position/orientation is treated as
	 *        a "virtual" object, where the system attempts to move the object to match the pose of 
	 *        this "virtual" object that has been placed at the target transform. Use this when you 
	 *        want to specify the target transform for the object as a whole. If false, then the 
	 *        target transform is used as is, and the system drives the control point towards this
	 *        transform.
	 * @return                  Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetPositionAndOrientation(
		FName            Name,
		const FVector    Position,
		const FRotator   Orientation,
		float            VelocityDeltaTime,
		bool             bEnableControl = true,
		bool             bApplyControlPointToTarget = false);

	/**
	 * Modifies an existing control target - i.e. what it is driving towards, relative to the parent object
	 *
	 * @param Name The name of the control to modify. 
	 * @param Position          The new position target for the control
	 * @param VelocityDeltaTime If non-zero, the target velocity will be calculated using the current target 
	 *        position. If zero, the target velocity will be set to zero.    
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @param bApplyControlPointToTarget If true, then the target position/orientation is treated as
	 *        a "virtual" object, where the system attempts to move the object
	 *        to match the pose of this "virtual" object that has been placed at
	 *        the target transform. Use this when you want to specify the target
	 *        transform for the object as a whole. If false, then the target transform
	 *        is used as is, and the system drives the control point towards this
	 *        transform.
	 * @return Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetPosition(
		FName         Name,
		const FVector Position, 
		float         VelocityDeltaTime, 
		bool          bEnableControl = true,
		bool          bApplyControlPointToTarget = false);

	/**
	 * Modifies an existing control target - i.e. what it is driving towards, relative to the parent object
	 *
	 * @param Name The name of the control to modify. 
	 * @param Orientation The new orientation target for the control
	 * @param AngularVelocityDeltaTime If non-zero, the target angular velocity will be calculated using the current 
	 *        target position. If zero, the target velocity will be set to zero.
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @param bApplyControlPointToTarget If true, then the target position/orientation is treated as
	 *        a "virtual" object, where the system attempts to move the object
	 *        to match the pose of this "virtual" object that has been placed at
	 *        the target transform. Use this when you want to specify the target
	 *        transform for the object as a whole. If false, then the target transform
	 *        is used as is, and the system drives the control point towards this
	 *        transform.
	 * @return Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetOrientation(
		FName          Name,
		const FRotator Orientation, 
		float          AngularVelocityDeltaTime, 
		bool           bEnableControl = true,
		bool           bApplyControlPointToTarget = false);

	/**
	 * Calculates and sets an existing control target. This takes the "virtual" position/orientation of the parent 
	 * and child and calculates the relative control. Note that this will set bApplyControlPointToTarget to true.
	 * 
	 * @param Name              The name of the control to modify. 
	 * @param ParentPosition    The virtual/target parent position
	 * @param ParentOrientation The virtual/target parent orientation
	 * @param ChildPosition     The virtual/target child position
	 * @param ChildOrientation  The virtual/target child orientation
	 * @param VelocityDeltaTime If non-zero, the target velocity will be calculated using the current target
	 *                          position. If zero, the target velocity will be set to zero.    
	 * @param bEnableControl    Enables the control if it is currently disabled
	 * @return                  Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetPoses(
		FName          Name,
		const FVector  ParentPosition, 
		const FRotator ParentOrientation,
		const FVector  ChildPosition, 
		const FRotator ChildOrientation,
		float          VelocityDeltaTime, 
		bool           bEnableControl = true);


	/**
	 * Sets whether or not the control should use skeletal animation for the targets
	 *
	 * @param Name The name of the control to modify. 
	 * @param bUseSkeletalAnimation If true then the targets will be a combination of the skeletal animation (if
	 *        there is any) and the control target that has been set
	 * @param SkeletalAnimationVelocityMultiplier If skeletal animation is being used, then this determines 
	 *        the amount of velocity extracted from the animation that is used as targets for the controls
	 * @return Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlUseSkeletalAnimation(
		FName Name,
		bool  bUseSkeletalAnimation = true,
		float SkeletalAnimationVelocityMultiplier = 1.0f);

	/**
	 * Sets whether or not the controls should use skeletal animation for the targets
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray 
	 *              then it can be split.
	 * @param bUseSkeletalAnimation If true then the targets will be a combination of the skeletal animation (if
	 *                              there is any) and the control target that has been set
	 * @param SkeletalAnimationVelocityMultiplier If skeletal animation is being used, then this determines the amount of 
	 *                              velocity extracted from the animation that is used as targets for the controls
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllControlsUseSkeletalAnimation(
		const TArray<FName>& Names,
		bool                 bUseSkeletalAnimation = true,
		float                SkeletalAnimationVelocityMultiplier = 1.0f);

	/**
	 * Activates or deactivates a control
	 *
	 * @param Name     The name of the control to modify. 
	 * @param bEnable  The control to enable/disable
	 * @return         Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlEnabled(FName Name, bool bEnable = true);

	/**
	 * Activates or deactivates controls
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param bEnable  The controls to enable/disable
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllControlsEnabled(const TArray<FName>& Names, bool bEnable = true);

	/**
	 * @param Name The name of the control to modify. 
	 * @param bAutoDisable If set then the control will automatically deactivate after each tick.
	 * @return     Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlAutoDisable(FName Name, bool bAutoDisable);

	/**
	 * @param Name     The name of the control to access. 
	 * @param Control  The control that will be filled in, if found
	 * @return         Returns true if the control was found, false if not
	 */
	//UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControl(FName Name, FPhysicsControl& Control) const;

	/**
	 * @param Name     The name of the control to access. 
	 * @param Control  The control data that will be filled in if found
	 * @return         Returns true if the control was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlData(FName Name, FPhysicsControlData& ControlData) const;

	/**
	 * @param Name     The name of the control to access. 
	 * @param Control  The control multipliers that will be filled in if found
	 * @return         Returns true if the control was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlMultipliers(FName Name, FPhysicsControlMultipliers& ControlMultipliers) const;

	/**
	 * @param Name     The name of the control to access. 
	 * @param Control  The control target, if found
	 * @return         Returns true if the control was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlTarget(FName Name, FPhysicsControlTarget& ControlTarget) const;

	/**
	 * @param Name        The name of the control to access. 
	 * @return            Returns true if the control is marked to automatically disable after each tick
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlAutoDisable(FName Name) const;

	/**
	 * @param Name        The name of the control to access. 
	 * @return            Returns true if the control is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlEnabled(FName Name) const;

	/**
	 * Makes a new body modifier for mesh components
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FName MakeBodyModifier(
		UMeshComponent*         MeshComponent,
		FName                   BoneName,
		EPhysicsMovementType    MovementType = EPhysicsMovementType::Simulated, 
		ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics,
		float                   GravityMultiplier = 1.0f,
		bool                    bUseSkeletalAnimation = true);

	/**
	 * Makes a new body modifier for mesh components
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool MakeNamedBodyModifier(
		FName                   Name,
		UMeshComponent*         MeshComponent,
		FName                   BoneName,
		EPhysicsMovementType    MovementType = EPhysicsMovementType::Simulated,
		ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics,
		float                   GravityMultiplier = 1.0f,
		bool                    bUseSkeletalAnimation = true);

	/**
	 * Makes new body modifiers for skeletal mesh components
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> MakeBodyModifiersFromSkeletalMeshBelow(
		USkeletalMeshComponent* SkeletalMeshComponent,
		FName                   BoneName,
		bool                    bIncludeSelf,
		EPhysicsMovementType    MovementType = EPhysicsMovementType::Simulated,
		ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics,
		float                   GravityMultiplier = 1.0f,
		bool                    bUseSkeletalAnimation = true);


	/**
	 * Makes a collection of controls controlling a skeletal mesh, grouped together in limbs
	 *
	 * @param AllControls A single container for all the controls that have been created
	 * @param LimbBones A map relating the limbs and the bones that they contain. Typically create this 
	 *                  using GetLimbBonesFromSkeletalMesh
	 * @param ControlType What type of control to create. This determines what the parent will be for each control
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlSettings General settings for the control
	 * @param bEnabled      If true then the control will be enabled immediately. If false you will need to call
	 *                      SetControlEnabled(true) in order to enable it.
	 * @return A map containing the controls for each limb
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TMap<FName, FPhysicsControlNameArray> MakeBodyModifiersFromLimbBones(
		FPhysicsControlNameArray&                    AllBodyModifiers,
		const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
		EPhysicsMovementType                         MovementType = EPhysicsMovementType::Simulated,
		ECollisionEnabled::Type                      CollisionType = ECollisionEnabled::QueryAndPhysics,
		float                                        GravityMultiplier = 1.0f,
		bool                                         bUseSkeletalAnimation = true);

	/**
	 * Destroys a BodyModifier
	 *
	 * @param Name        The name of the body modifier to destroy. 
	 * @return            Returns true if the body modifier was found and destroyed, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool DestroyBodyModifier(FName Name);

	/**
	 * Destroys BodyModifiers
	 *
	 * @param Names The names of the body modifiers to destroy. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void DestroyAllBodyModifiers(const TArray<FName>& Names);

	/**
	 * Sets the kinematic target transform for a body modifier. 
	 * 
	 * @param Name The name of the body modifier to access. 
	 * @param KinematicTargetPosition The position to use as the kinematic target of the associated body,
	 *                                if it is kinematic
	 * @param KinematicTargetOrientation The orientation to use as the kinematic target of the associated body,
	 *                                   if it is kinematic
	 * @param bMakeKinematic If set then the body will be made kinematic. If not set, then it won't be changed.
	 * @return Returns true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierKinematicTarget(
		FName    Name,
		FVector  KinematicTargetPosition, 
		FRotator KinematicTargetOrienation,
		bool     bMakeKinematic);

	/**
	 * Sets the movement type for a body modifier
	 *
	 * @param Name The name of the body modifier to access. 
	 * @param MovementType Whether to enable/disable simulation on the body
	 * @return Returns true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierMovementType(
		FName                Name,
		EPhysicsMovementType MovementType = EPhysicsMovementType::Simulated);

	/**
	 * Sets the movement type for body modifiers
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *        then it can be split.
	 * @param MovementType Whether to enable/disable simulation on the bodies
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllBodyModifierMovementType(
		const TArray<FName>& Names,
		EPhysicsMovementType MovementType = EPhysicsMovementType::Simulated);

	/**
	 * Sets the collision type for a body modifier
	 *
	 * @param Name The name of the body modifier to access.
	 * @param CollisionType Collision type to set on the body
	 * @return Returns true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierCollisionType(
		FName                   Name,
		ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics);

	/**
	 * Sets the collision type for body modifiers
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *        then it can be split.
	 * @param CollisionType Collision type to set on the bodies
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllBodyModifierCollisionType(
		const TArray<FName>&    Names,
		ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics);

	/**
	 * Sets the gravity multiplier for a body modifier
	 *
	 * @param Name The name of the body modifier to access. 
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 * @return Returns true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierGravityMultiplier(
		FName Name,
		float GravityMultiplier = 1.0f);

	/**
	 * Sets the gravity multiplier for body modifiers
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllBodyModifierGravityMultipliers(
		const TArray<FName>& Names,
		float                GravityMultiplier = 1.0f);

	/**
	 * Sets whether a body modifier should use skeletal animation for its kinematic targets
	 *
	 * @param Name The name of the body modifier to access. 
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the 
	 *                              skeletal animation, rather than world space. Only relevant if the
	 *                              body is part of a skeletal mesh.
	 * @return Returns true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierUseSkeletalAnimation(
		FName Name,
		bool  bUseSkeletalAnimation);

	/**
	 * Sets whether body modifiers should use skeletal animation for their kinematic targets
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the 
	 *                              skeletal animation, rather than world space. Only relevant if the
	 *                              body is part of a skeletal mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetAllBodyModifiersUseSkeletalAnimation(
		const TArray<FName>& Names,
		bool                 bUseSkeletalAnimation);

public:

	/**
	 * If the component moves by more than this distance then it is treated as a teleport,
	 * which prevents velocities being used for a frame. It is also used as the threshold for
	 * teleporting when moving kinematic objects. Zero or negative disables.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Teleport)
	float TeleportDistanceThreshold;

	/**
	 * If the component rotates by more than this angle (in degrees) then it is treated as a teleport,
	 * which prevents velocities being used for a frame. It is also used as the threshold for
	 * teleporting when moving kinematic objects. Zero or negative disables.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Teleport)
	float TeleportRotationThreshold;

	/** Visualize the controls when this actor/component is selected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	bool bShowDebugVisualization;

	/** Size of the gizmos etc used during visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	float VisualizationSizeScale;

	/**
	 * The time used when "predicting" the target position/orientation. Zero will disable the visualization
	 * of this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	float VelocityPredictionTime;

	/**
	 * Upper limit on the number of controls or modifiers that will be created using the same name (which
	 * will get a numerical postfix). When this limit is reached a warning will be issued  and the control 
	 * or modifier won't be created. This is to avoid problems if controls or modifiers are being created 
	 * dynamically, and can generally be a "moderately large" number, depending on how many controls or 
	 * modifiers you expect to create.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	int32 MaxNumControlsOrModifiersPerName;

protected:

	// Begin UActorComponent Interface
	virtual void InitializeComponent() override;
	virtual void BeginDestroy() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent Interface

public:
#if WITH_EDITOR
	// Used by the component visualizer
	void DebugDraw(FPrimitiveDrawInterface* PDI) const;
	void DebugDrawControl(FPrimitiveDrawInterface* PDI, const FPhysicsControlRecord& Record, FName ControlName) const;
#endif

protected:

	TPimplPtr<FPhysicsControlComponentImpl> Implementation;
};
