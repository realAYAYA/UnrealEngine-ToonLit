// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlData.h"
#include "PhysicsControlLimbData.h"

#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
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
 * Specifies how any reset to cached target should work. 
 */
UENUM(BlueprintType)
enum class EResetToCachedTargetBehavior : uint8
{
	/** 
	 * Reset of the associated physics bodies is done immediately, to whatever transforms are in the cache. These
	 * will reflect the previous or the upcoming animation targets, depending on what stage of the tick this 
	 * is called.
	 */
	ResetImmediately,
	/**
	 * Reset of the associated physics bodies will be done during the next Tick (or UpdateControls). Note that
	 * this will reset the bodies to the updated animation targets, and then there will be a subsequent physics 
	 * update, which will result in the final transforms.
	 */
	ResetDuringUpdateControls
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
class PHYSICSCONTROL_API UPhysicsControlComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	/**
	* Allows manual ticking so that your code can run in between updating the target caches and updating 
	* the controls. This allows you to read the targets coming from animation and use those values to 
	* create your own controls etc.
	* 
	* To use this function, you should disable ticking of the Physics Control Component, and ensure that the 
	* relevant Skeletal Mesh Component (if being used) has ticked, using a tick prerequisite. Then explicitly 
	* call (in order) UpdateTargetCaches and UpdateControls as you process your tick.
	*/
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void UpdateTargetCaches(float DeltaTime);

	/**
	* Allows manual ticking so that your code can run in between updating the target caches and updating
	* the controls and body modifiers. This allows you to read the targets coming from animation and use 
	* those values to create your own controls etc.
	*
	* To use this function, you should disable ticking of the Physics Control Component, and ensure that the
	* relevant Skeletal Mesh Component (if being used) has ticked, using a tick prerequisite. Then explicitly
	* call (in order) UpdateTargetCaches and UpdateControls as you process your tick.
	*/
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void UpdateControls(float DeltaTime);

	/**
	 * Creates a new control for mesh components
	 * 
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlTarget Describes the initial target for the new control
	 * @param ControlSettings General settings for the control
	 * @param Set Which set to include the control in (optional). Note that it automatically
	 *        gets added to the set "All"
	 * @param bEnabled If true then the control will be enabled immediately. If false you will need to call
	 *        SetControlEnabled(true) in order to activate it.
	 * @return The name of the new control
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FName CreateControl(
		UMeshComponent*               ParentMeshComponent,
		FName                         ParentBoneName,
		UMeshComponent*               ChildMeshComponent,
		const FName                   ChildBoneName,
		const FPhysicsControlData     ControlData,
		const FPhysicsControlTarget   ControlTarget, 
		const FPhysicsControlSettings ControlSettings,
		FName                         Set,
		const bool                    bEnabled = true
	);

	/**
	 * Creates a new control for mesh components
	 * 
	 * @param The name of the control that will be created. Creation will fail if this name is already in use.
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlTarget Describes the initial target for the new control
	 * @param ControlSettings General settings for the control
	 * @param Set Which set to include the control in (optional). Note that it automatically
	 *        gets added to the set "All"
	 * @param bEnabled If true then the control will be enabled immediately. If false you will need to call
	 *        SetControlEnabled(true) in order to enable it.
	 * @return True if a new control was created, false if a control of the specified name already exists
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool CreateNamedControl(
		FName                         Name,
		UMeshComponent*               ParentMeshComponent,
		const FName                   ParentBoneName,
		UMeshComponent*               ChildMeshComponent,
		const FName                   ChildBoneName,
		const FPhysicsControlData     ControlData, 
		const FPhysicsControlTarget   ControlTarget, 
		const FPhysicsControlSettings ControlSettings, 
		const FName                   Set,
		const bool                    bEnabled = true);

	/**
	 * Creates a collection of controls controlling a skeletal mesh
	 * 
	 * @param SkeletalMeshComponent The skeletal mesh which will have controls
	 * @param BoneName The name of the bone below which controls should be created. Each bone will be the child in a control
	 * @param bIncludeSelf Whether or not to include BoneName when creating controls
	 * @param ControlType What type of control to create. This determines what the parent will be for each control
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlSettings General settings for the control
	 * @param Set Which set to include the control in (optional). Note that it automatically
	 *        gets added to the set "All"
	 * @param bEnabled If true then the control will be enabled immediately. If false you will need to call
	 *        SetControlEnabled(true) in order to enable it.
	 * @return An array of the controls that have been created
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> CreateControlsFromSkeletalMeshBelow(
		USkeletalMeshComponent*       SkeletalMeshComponent,
		const FName                   BoneName,
		const bool                    bIncludeSelf,
		const EPhysicsControlType     ControlType,
		const FPhysicsControlData     ControlData,
		const FPhysicsControlSettings ControlSettings,
		const FName                   Set,
		const bool                    bEnabled = true);

	/**
	 * Creates a collection of ParentSpace controls controlling a skeletal mesh, initializing
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
	 * @param Set Which set to include the control in (optional). Note that it automatically
	 *        gets added to the set "All"
	 * @param bEnabled If true then the control will be enabled immediately. If false you will need to call
	 *        SetControlEnabled(true) in order to enable it.
	 * @return An array of the controls that have been created
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> CreateControlsFromSkeletalMeshAndConstraintProfileBelow(
		USkeletalMeshComponent* SkeletalMeshComponent,
		const FName             BoneName,
		const bool              bIncludeSelf,
		const FName             ConstraintProfile,
		const FName             Set,
		const bool              bEnabled = true);

	/**
	 * Creates a collection of controls controlling a skeletal mesh
	 *
	 * @param SkeletalMeshComponent The skeletal mesh which will have controls
	 * @param BoneNames The names of bones for which controls should be created. Each bone will be the child in a control
	 * @param ControlType What type of control to create. This determines what the parent will be for each control
	 * @param ControlData   Describes the initial strength etc of the new control
	 * @param ControlSettings General settings for the control
	 * @param Set Which set to include the control in (optional). Note that it automatically
	 *        gets added to the set "All"
	 * @param bEnabled If true then the control will be enabled immediately. If false you will need to call
	 *        SetControlEnabled(true) in order to enable it.
	 * @return An array of the controls that have been created
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> CreateControlsFromSkeletalMesh(
		USkeletalMeshComponent*       SkeletalMeshComponent,
		const TArray<FName>&          BoneNames,
		const EPhysicsControlType     ControlType,
		const FPhysicsControlData     ControlData,
		const FPhysicsControlSettings ControlSettings,
		const FName                   Set,
		const bool                    bEnabled = true);

	/**
	 * Creates a collection of ParentSpace controls controlling a skeletal mesh, initializing them 
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
	 * @param Set Which set to include the control in (optional). Note that it automatically
	 *        gets added to the set "All"
	 * @param bEnabled If true then the control will be enabled immediately. If false you will need to call
	 *        SetControlEnabled(true) in order to enable it.
	 * @return An array of the controls that have been created
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> CreateControlsFromSkeletalMeshAndConstraintProfile(
		USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FName>&    BoneNames,
		const FName             ConstraintProfile,
		const FName             Set,
		const bool              bEnabled = true);

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
		USkeletalMeshComponent*                     SkeletalMeshComponent,
		const TArray<FPhysicsControlLimbSetupData>& LimbSetupData) const;

	/**
	 * Creates a collection of controls controlling a skeletal mesh, grouped together in limbs
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
	TMap<FName, FPhysicsControlNames> CreateControlsFromLimbBones(
		FPhysicsControlNames&                        AllControls,
		const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
		const EPhysicsControlType                    ControlType,
		const FPhysicsControlData                    ControlData,
		const FPhysicsControlSettings                ControlSettings,
		const bool                                   bEnabled = true);

	/**
	 * Creates a collection of ParentSpace controls controlling a skeletal mesh, grouped together in limbs, initializing
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
	TMap<FName, FPhysicsControlNames> CreateControlsFromLimbBonesAndConstraintProfile(
		FPhysicsControlNames&                        AllControls,
		const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
		const FName                                  ConstraintProfile,
		const bool                                   bEnabled = true);

	/**
	 * Destroys a control
	 *
	 * @param Name The name of the control to destroy. 
	 * @return     Returns true if the control was found and destroyed, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool DestroyControl(const FName Name);

	/**
	 * Destroys all controls
	 *
	 * @param Names The names of the controls to destroy. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void DestroyControls(const TArray<FName>& Names);

	/**
	 * Destroys all controls in a set
	 *
	 * @param Set The set of controls to use to destroy. Standard sets will include "All", "WorldSpace",
	 * "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void DestroyControlsInSet(const FName Set);

	/**
	 * Modifies an existing control data - i.e. the strengths etc of the control driving towards the target
	 *
	 * @param Name The name of the control to modify. 
	 * @param ControlData The new control data
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlData(const FName Name, const FPhysicsControlData ControlData, const bool bEnableControl = true);

	/**
	 * Modifies existing control data - i.e. the strengths etc of the controls driving towards the targets
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param ControlData The new control data
	 * @param bEnableControl Enables the control if it is currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlDatas(const TArray<FName>& Names, const FPhysicsControlData ControlData, const bool bEnableControl = true);

	/**
	 * Modifies existing control data - i.e. the strengths etc of the controls driving towards the targets
	 *
	 * @param Set The set of controls to modify. Standard sets will include "All", "WorldSpace",
	 *        "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 * @param ControlData The new control data
	 * @param bEnableControl Enables the controls if currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlDatasInSet(const FName Set, const FPhysicsControlData ControlData, const bool bEnableControl = true);

	/**
	 * Modifies an existing control data using the multipliers
	 *
	 * @param Name The name of the control to modify. 
	 * @param ControlMultipliers The new control multipliers
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlMultiplier(const FName Name, const FPhysicsControlMultiplier ControlMultiplier, const bool bEnableControl = true);

	/**
	 * Modifies existing control data using the multipliers
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param ControlMultiplier The new control multiplier
	 * @param bEnableControl Enables the controls if currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlMultipliers(
		const TArray<FName>&            Names, 
		const FPhysicsControlMultiplier ControlMultiplier, 
		const bool                      bEnableControl = true);

	/**
	 * Modifies existing control data using the multipliers
	 *
	 * @param Set The set of controls to modify. Standard sets will include "All", "WorldSpace",
	 *        "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 * @param ControlMultiplier The new control multiplier
	 * @param bEnableControl Enables the controls if currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlMultipliersInSet(
		const FName                      Set, 
		const FPhysicsControlMultiplier  ControlMultiplier, 
		const bool                       bEnableControl = true);

	/**
	 * Modifies an existing control's linear data - i.e. the strengths etc of the control driving towards the target
	 *
	 * @param Name The name of the control to modify. 
	 * @param Strength The strength used to drive linear motion
	 * @param DampingRatio The amount of damping associated with the linear strength. 1 Results in critically damped motion
	 * @param ExtraDamping The amount of additional linear damping
	 * @param MaxForce The maximum force used to drive the linear motion. Zero indicates no limit.
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlLinearData(
		const FName Name,
		const float Strength = 1.0f, 
		const float DampingRatio = 1.0f, 
		const float ExtraDamping = 0.0f, 
		const float MaxForce = 0.0f, 
		const bool  bEnableControl = true);

	/**
	 * Modifies an existing control's angular data - i.e. the strengths etc of the control driving towards the target
	 *
	 * @param Name The name of the control to modify. 
	 * @param Strength The strength used to drive angular motion
	 * @param DampingRatio The amount of damping associated with the angular strength. 1 Results in critically damped motion
	 * @param ExtraDamping The amount of additional angular damping
	 * @param MaxTorque The maximum torque used to drive the angular motion. Zero indicates no limit.
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlAngularData(
		const FName Name,
		const float Strength = 1.0f, 
		const float DampingRatio = 1.0f, 
		const float ExtraDamping = 0.0f, 
		const float MaxTorque = 0.0f, 
		const bool  bEnableControl = true);

	/**
	 * Sets the point at which controls will "push" the child object.
	 * 
	 * @param Name The name of the control to modify. 
	 * @param Position The position of the control point on the child mesh object (only relevant if that 
	 *        object is in use and is being simulated)
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlPoint(const FName Name, const FVector Position);

	/**
	 * Resets the control point to the center of mass of the mesh
	 *
	 * @param Name The name of the control to modify. 
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool ResetControlPoint(const FName Name);

	/**
	 * Modifies an existing control target - i.e. what it is driving towards, relative to the parent object
	 *
	 * @param Name The name of the control to modify. 
	 * @param ControlTarget The new target for the control
	 * @param bEnableControl Enables the control if it is currently disabled
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTarget(const FName Name, const FPhysicsControlTarget ControlTarget, const bool bEnableControl = true);

	/**
	 * Modifies existing control targets - i.e. what they are driving towards, relative to the parent objects
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param ControlTarget The new target for the controls
	 * @param bEnableControl Enables the controls if currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlTargets(
		const TArray<FName>&        Names, 
		const FPhysicsControlTarget ControlTarget, 
		const bool                  bEnableControl = true);

	/**
	 * Modifies existing control targets - i.e. what they are driving towards, relative to the parent objects
	 *
	 * @param Set The set of controls to modify. Standard sets will include "All", "WorldSpace",
	 *        "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 * @param ControlTarget The new target for the controls
	 * @param bEnableControl Enables the controls if currently disabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlTargetsInSet(
		const FName                 Set, 
		const FPhysicsControlTarget ControlTarget, 
		const bool                  bEnableControl = true);

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
		const FName    Name,
		const FVector  Position,
		const FRotator Orientation,
		const float    VelocityDeltaTime,
		const bool     bEnableControl = true,
		const bool     bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetPositionAndOrientation for each of the control names
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlTargetPositionsAndOrientations(
		const TArray<FName>& Names,
		const FVector        Position,
		const FRotator       Orientation,
		const float          VelocityDeltaTime,
		const bool           bEnableControl = true,
		const bool           bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetPositionAndOrientation for each control in the set
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlTargetPositionsAndOrientationsInSet(
		const FName          SetName,
		const FVector        Position,
		const FRotator       Orientation,
		const float          VelocityDeltaTime,
		const bool           bEnableControl = true,
		const bool           bApplyControlPointToTarget = false);

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
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetPosition(
		const FName   Name,
		const FVector Position, 
		const float   VelocityDeltaTime, 
		const bool    bEnableControl = true,
		const bool    bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetPosition for each of the control names
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlTargetPositions(
		const TArray<FName>& Names,
		const FVector        Position, 
		const float          VelocityDeltaTime, 
		const bool           bEnableControl = true,
		const bool           bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetPosition for each of the controls in the set
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlTargetPositionsInSet(
		const FName   SetName,
		const FVector Position, 
		const float   VelocityDeltaTime, 
		const bool    bEnableControl = true,
		const bool    bApplyControlPointToTarget = false);

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
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetOrientation(
		const FName    Name,
		const FRotator Orientation, 
		const float    AngularVelocityDeltaTime, 
		const bool     bEnableControl = true,
		const bool     bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetOrientation for each of the control names
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlTargetOrientations(
		const TArray<FName>& Names,
		const FRotator Orientation,
		const float    AngularVelocityDeltaTime, 
		const bool     bEnableControl = true,
		const bool     bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetOrientation for each of the controls in the set
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlTargetOrientationsInSet(
		const FName    SetName,
		const FRotator Orientation, 
		const float    AngularVelocityDeltaTime, 
		const bool     bEnableControl = true,
		const bool     bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetPosition for each element of the control names and positions. These array should match
	 * in size.
	 * @return true if the control/position arrays match, false if they don't.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetPositionsFromArray(
		const TArray<FName>&   Names,
		const TArray<FVector>& Positions, 
		const float            VelocityDeltaTime, 
		const bool             bEnableControl = true,
		const bool             bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetPosition for each element of the control names and positions. These array should match
	 * in size.
	 * @return true if the control/position arrays match, false if they don't.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetOrientationsFromArray(
		const TArray<FName>&    Names,
		const TArray<FRotator>& Orientations, 
		const float             VelocityDeltaTime, 
		const bool              bEnableControl = true,
		const bool              bApplyControlPointToTarget = false);

	/**
	 * Calls SetControlTargetPositionAndOrientation for each element of the control names, positions and orientations. 
	 * These array should match in size.
	 * @return true if the control/position/orientation arrays match, false if they don't.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlTargetPositionsAndOrientationsFromArray(
		const TArray<FName>&    Names,
		const TArray<FVector>&  Positions,
		const TArray<FRotator>& Orientations,
		const float             VelocityDeltaTime, 
		const bool              bEnableControl = true,
		const bool              bApplyControlPointToTarget = false);

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
		const FName    Name,
		const FVector  ParentPosition, 
		const FRotator ParentOrientation,
		const FVector  ChildPosition, 
		const FRotator ChildOrientation,
		const float    VelocityDeltaTime, 
		const bool     bEnableControl = true);


	/**
	 * Sets whether or not the control should use skeletal animation for the targets
	 *
	 * @param Name The name of the control to modify. 
	 * @param bUseSkeletalAnimation If true then the targets will be a combination of the skeletal animation (if
	 *        there is any) and the control target that has been set
	 * @param SkeletalAnimationVelocityMultiplier If skeletal animation is being used, then this determines 
	 *        the amount of velocity extracted from the animation that is used as targets for the controls
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlUseSkeletalAnimation(
		const FName Name,
		const bool  bUseSkeletalAnimation = true,
		const float SkeletalAnimationVelocityMultiplier = 1.0f);

	/**
	 * Sets whether or not the controls should use skeletal animation for the targets
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray 
	 *              then it can be split.
	 * @param bUseSkeletalAnimation If true then the targets will be a combination of the skeletal animation (if
	 *              there is any) and the control target that has been set
	 * @param SkeletalAnimationVelocityMultiplier If skeletal animation is being used, then this determines the amount of 
	 *              velocity extracted from the animation that is used as targets for the controls
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlsUseSkeletalAnimation(
		const TArray<FName>& Names,
		const bool           bUseSkeletalAnimation = true,
		const float          SkeletalAnimationVelocityMultiplier = 1.0f);

	/**
	 * Sets whether or not the controls should use skeletal animation for the targets
	 *
	 * @param Set The set of controls to modify. Standard sets will include "All", "WorldSpace",
	 *        "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 * @param bUseSkeletalAnimation If true then the targets will be a combination of the skeletal animation (if
	 *        there is any) and the control target that has been set
	 * @param SkeletalAnimationVelocityMultiplier If skeletal animation is being used, then this determines the amount of
	 *        velocity extracted from the animation that is used as targets for the controls
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlsInSetUseSkeletalAnimation(
		const FName Set,
		const bool  bUseSkeletalAnimation = true,
		const float SkeletalAnimationVelocityMultiplier = 1.0f);

	/**
	 * Activates or deactivates a control
	 *
	 * @param Name     The name of the control to modify. 
	 * @param bEnable  Whether to enable/disable the control
	 * @return         Returns true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlEnabled(const FName Name, const bool bEnable = true);

	/**
	 * Activates or deactivates controls
	 *
	 * @param Names The names of the controls to modify. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param bEnable  Whether to enable/disable the controls
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlsEnabled(const TArray<FName>& Names, const bool bEnable = true);

	/**
	 * Activates or deactivates controls
	 *
	 * @param Set The set of controls to modify. Standard sets will include "All", "WorldSpace",
	 *        "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 * @param bEnable  Whether to enable/disable the controls
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlsInSetEnabled(const FName Set, const bool bEnable = true);

	/**
	 * @param Name The name of the control to modify. 
	 * @param bAutoDisable If set then the control will automatically deactivate after each tick.
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlAutoDisable(const FName Name, const bool bAutoDisable);

	/**
	 * @param Names The names of the controls to modify. 
	 * @param bAutoDisable If set then the control will automatically deactivate after each tick.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlsAutoDisable(const TArray<FName>& Names, const bool bAutoDisable);

	/**
	 * @param Set The set of controls to modify. Standard sets will include "All", "WorldSpace",
	 *        "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 * @param bAutoDisable If set then the control will automatically deactivate after each tick.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlsInSetAutoDisable(const FName Set, const bool bAutoDisable);

	/**
	 * @param Name The name of the control to modify. 
	 * @param bDisableCollision If set then the control will disable collision between the bodies it connects.
	 * @return true if the control was found and modified, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetControlDisableCollision(const FName Name, const bool bDisableCollision);

	/**
	 * @param Names The names of the controls to modify. 
	 * @param bDisableCollision If set then the control will disable collision between the bodies it connects.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlsDisableCollision(const TArray<FName>& Names, const bool bDisableCollision);

	/**
	 * @param Set The set of controls to modify. Standard sets will include "All", "WorldSpace",
	 *        "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 * @param bDisableCollision If set then the control will disable collision between the bodies it connects.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetControlsInSetDisableCollision(const FName Set, const bool bDisableCollision);

	/**
	 * @param Name     The name of the control to access. 
	 * @param Control  The control data that will be filled in if found
	 * @return         Returns true if the control was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlData(const FName Name, FPhysicsControlData& ControlData) const;

	/**
	 * @param Name     The name of the control to access. 
	 * @param Control  The control multipliers that will be filled in if found
	 * @return         Returns true if the control was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlMultiplier(const FName Name, FPhysicsControlMultiplier& ControlMultiplier) const;

	/**
	 * @param Name     The name of the control to access. 
	 * @param Control  The control target, if found
	 * @return         Returns true if the control was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlTarget(const FName Name, FPhysicsControlTarget& ControlTarget) const;

	/**
	 * @param Name        The name of the control to access. 
	 * @return            Returns true if the control is marked to automatically disable after each tick
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlAutoDisable(const FName Name) const;

	/**
	 * @param Name        The name of the control to access. 
	 * @return            Returns true if the control is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool GetControlEnabled(const FName Name) const;

	/**
	 * Creates a new body modifier for mesh components
	 * 
	 * @param MeshComponent The Mesh Component used as a target for the modifier
	 * @param BoneName The bone name, if a skeletal mesh is used
	 * @param Set Which set to include the body modifier in (optional). Note that it automatically 
	 *        gets added to the set "All"
	 * @param MovementType Whether to enable/disable simulation on the body
	 * @param CollisionType Collision type to set on the body
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 * @param PhysicsBlendWeight The blend weight between the body transform coming from
	 *        animation and that coming from simulation.
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the
	 *        skeletal animation, rather than world space. Only relevant if the body is part of 
	 *        a skeletal mesh.
	 * @param bUpdateKinematicFromSimulation Whether the body should be updated from the simulation when
	 *        it is kinematic, or whether it should track the kinematic target directly. This will be most
	 *        likely useful when using async physics, in order to make kinematic parts behave the same as 
	 *        dynamic ones.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FName CreateBodyModifier(
		UMeshComponent*               MeshComponent,
		const FName                   BoneName,
		const FName                   Set,
		const EPhysicsMovementType    MovementType = EPhysicsMovementType::Simulated,
		const ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics,
		const float                   GravityMultiplier = 1.0f,
		const float                   PhysicsBlendWeight = 1.0f,
		const bool                    bUseSkeletalAnimation = true,
		const bool                    bUpdateKinematicFromSimulation = true);

	/**
	 * Creates a new body modifier for mesh components
	 * 
	 * @param The name of the body modifier that will be created. Creation will fail if this name is already in use.
	 * @param MeshComponent The Mesh Component used as a target for the modifier
	 * @param BoneName The bone name, if a skeletal mesh is used
	 * @param Set Which set to include the body modifier in (optional). Note that it automatically
	 *        gets added to the set "All"
	 * @param MovementType Whether to enable/disable simulation on the body
	 * @param CollisionType Collision type to set on the body
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 * @param PhysicsBlendWeight The blend weight between the body transform coming from
	 *        animation and that coming from simulation.
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the
	 *        skeletal animation, rather than world space. Only relevant if the body is part of 
	 *        a skeletal mesh.
	 * @param bUpdateKinematicFromSimulation Whether the body should be updated from the simulation when
	 *        it is kinematic, or whether it should track the kinematic target directly. This will be most
	 *        likely useful when using async physics, in order to make kinematic parts behave the same as
	 *        dynamic ones.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool CreateNamedBodyModifier(
		const FName                   Name,
		UMeshComponent*               MeshComponent,
		const FName                   BoneName,
		const FName                   Set,
		const EPhysicsMovementType    MovementType = EPhysicsMovementType::Simulated,
		const ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics,
		const float                   GravityMultiplier = 1.0f,
		const float                   PhysicsBlendWeight = 1.0f,
		const bool                    bUseSkeletalAnimation = true,
		const bool                    bUpdateKinematicFromSimulation = true);

	/**
	 * Creates new body modifiers for skeletal mesh components
	 * 
	 * @param SkeletalMeshComponent The skeletal mesh which will have body modifiers
	 * @param BoneName The bone name below which modifiers should be created
	 * @param bIncludeSelf Whether or not to include BoneName when creating modifiers
	 * @param Set Which set to include the body modifier in (optional). Note that it automatically
	 *        gets added to the set "All"
	 * @param MovementType Whether to enable/disable simulation on the body
	 * @param CollisionType Collision type to set on the body
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 * @param PhysicsBlendWeight The blend weight between the body transform coming from
	 *        animation and that coming from simulation.
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the
	 *        skeletal animation, rather than world space. Only relevant if the body is part of
	 *        a skeletal mesh.
	 * @param bUpdateKinematicFromSimulation Whether the body should be updated from the simulation when
	 *        it is kinematic, or whether it should track the kinematic target directly. This will be most
	 *        likely useful when using async physics, in order to make kinematic parts behave the same as
	 *        dynamic ones.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> CreateBodyModifiersFromSkeletalMeshBelow(
		USkeletalMeshComponent*       SkeletalMeshComponent,
		const FName                   BoneName,
		const bool                    bIncludeSelf,
		const FName                   Set,
		const EPhysicsMovementType    MovementType = EPhysicsMovementType::Simulated,
		const ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics,
		const float                   GravityMultiplier = 1.0f,
		const float                   PhysicsBlendWeight = 1.0f,
		const bool                    bUseSkeletalAnimation = true,
		const bool                    bUpdateKinematicFromSimulation = true);


	/**
	 * Creates a collection of controls controlling a skeletal mesh, grouped together in limbs
	 *
	 * @param AllControls A single container for all the controls that have been created
	 * @param LimbBones A map relating the limbs and the bones that they contain. Typically create this 
	 *                  using GetLimbBonesFromSkeletalMesh
	 * @param MovementType Whether to enable/disable simulation on the body
	 * @param CollisionType Collision type to set on the body
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 * @param PhysicsBlendWeight The blend weight between the body transform coming from
	 *        animation and that coming from simulation.
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the
	 *        skeletal animation, rather than world space. Only relevant if the body is part of
	 *        a skeletal mesh.
	 * @param bUpdateKinematicFromSimulation Whether the body should be updated from the simulation when
	 *        it is kinematic, or whether it should track the kinematic target directly. This will be most
	 *        likely useful when using async physics, in order to make kinematic parts behave the same as
	 *        dynamic ones.
	 *
	 * @return A map containing the modifiers for each limb
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TMap<FName, FPhysicsControlNames> CreateBodyModifiersFromLimbBones(
		FPhysicsControlNames&                        AllBodyModifiers,
		const TMap<FName, FPhysicsControlLimbBones>& LimbBones,
		const EPhysicsMovementType                   MovementType = EPhysicsMovementType::Simulated,
		const ECollisionEnabled::Type                CollisionType = ECollisionEnabled::QueryAndPhysics,
		const float                                  GravityMultiplier = 1.0f,
		const float                                  PhysicsBlendWeight = 1.0f,
		const bool                                   bUseSkeletalAnimation = true,
		const bool                                   bUpdateKinematicFromSimulation = true);

	/**
	 * Destroys a BodyModifier
	 *
	 * @param Name        The name of the body modifier to destroy. 
	 * @return            Returns true if the body modifier was found and destroyed, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool DestroyBodyModifier(const FName Name);

	/**
	 * Destroys BodyModifiers
	 *
	 * @param Names The names of the body modifiers to destroy. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void DestroyBodyModifiers(const TArray<FName>& Names);

	/**
	 * Destroys BodyModifiers
	 *
	 * @param Set The set of body modifiers to destroy. Standard sets will include "All" and things like
	 *        "ArmLeft", depending on how body modifiers have been created.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void DestroyBodyModifiersInSet(const FName Set);

	/**
	 * Sets the kinematic target transform for a body modifier. 
	 * 
	 * @param Name The name of the body modifier to access. 
	 * @param KinematicTargetPosition The position to use as the kinematic target of the associated body,
	 *                                if it is kinematic
	 * @param KinematicTargetOrientation The orientation to use as the kinematic target of the associated body,
	 *                                   if it is kinematic
	 * @param bMakeKinematic If set then the body will be made kinematic. If not set, then it won't be changed.
	 * @return true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierKinematicTarget(
		const FName    Name,
		const FVector  KinematicTargetPosition, 
		const FRotator KinematicTargetOrienation,
		const bool     bMakeKinematic);

	/**
	 * Sets the movement type for a body modifier
	 *
	 * @param Name The name of the body modifier to access. 
	 * @param MovementType Whether to enable/disable simulation on the body
	 * @return true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierMovementType(
		const FName                Name,
		const EPhysicsMovementType MovementType = EPhysicsMovementType::Simulated);

	/**
	 * Sets the movement type for body modifiers
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *        then it can be split.
	 * @param MovementType Whether to enable/disable simulation on the bodies
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersMovementType(
		const TArray<FName>&       Names,
		const EPhysicsMovementType MovementType = EPhysicsMovementType::Simulated);

	/**
	 * Sets the movement type for body modifiers
	 *
	 * @param Set The set of body modifiers to modify. Standard sets will include "All" and things like
	 *        "ArmLeft", depending on how body modifiers have been created.
	 * @param MovementType Whether to enable/disable simulation on the bodies
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersInSetMovementType(
		const FName                Set,
		const EPhysicsMovementType MovementType = EPhysicsMovementType::Simulated);

	/**
	 * Sets the collision type for a body modifier
	 *
	 * @param Name The name of the body modifier to access.
	 * @param CollisionType Collision type to set on the body
	 * @return true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierCollisionType(
		const FName                   Name,
		const ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics);

	/**
	 * Sets the collision type for body modifiers
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *        then it can be split.
	 * @param CollisionType Collision type to set on the bodies
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersCollisionType(
		const TArray<FName>&          Names,
		const ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics);

	/**
	 * Sets the collision type for body modifiers
	 *
	 * @param Set The set of body modifiers to modify. Standard sets will include "All" and things like
	 *        "ArmLeft", depending on how body modifiers have been created.
	 * @param CollisionType Collision type to set on the bodies
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersInSetCollisionType(
		const FName                   Set,
		const ECollisionEnabled::Type CollisionType = ECollisionEnabled::QueryAndPhysics);

	/**
	 * Sets the gravity multiplier for a body modifier
	 *
	 * @param Name The name of the body modifier to access. 
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 * @return true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierGravityMultiplier(
		const FName Name,
		const float GravityMultiplier = 1.0f);

	/**
	 * Sets the gravity multiplier for body modifiers
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *              then it can be split.
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersGravityMultiplier(
		const TArray<FName>& Names,
		const float          GravityMultiplier = 1.0f);

	/**
	 * Sets the gravity multiplier for body modifiers
	 *
	 * @param Set The set of body modifiers to modify. Standard sets will include "All" and things like
	 *        "ArmLeft", depending on how body modifiers have been created.
	 * @param GravityMultiplier The amount of gravity to apply when simulating
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersInSetGravityMultiplier(
		const FName Set,
		const float GravityMultiplier = 1.0f);

	/**
	 * Sets the physics blend weight for a body modifier
	 *
	 * @param Name The name of the body modifier to access. 
	 * @param PhysicsBlendWeight The blend weight between the body transform coming from 
	 *        animation and that coming from simulation.
	 * @return true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierPhysicsBlendWeight(
		const FName Name,
		const float PhysicsBlendWeight = 1.0f);

	/**
	 * Sets the physics blend weight for body modifiers
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *        then it can be split.
	 * @param PhysicsBlendWeight The blend weight between the body transform coming from
	 *        animation and that coming from simulation.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersPhysicsBlendWeight(
		const TArray<FName>& Names,
		const float          PhysicsBlendWeight = 1.0f);

	/**
	 * Sets the physics blend weight for body modifiers
	 *
	 * @param Set The set of body modifiers to modify. Standard sets will include "All" and things like
	 *        "ArmLeft", depending on how body modifiers have been created.
	 * @param PhysicsBlendWeight The blend weight between the body transform coming from
	 *        animation and that coming from simulation.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersInSetPhysicsBlendWeight(
		const FName Set,
		const float PhysicsBlendWeight = 1.0f);

	/**
	 * Sets whether a body modifier should use skeletal animation for its kinematic targets
	 *
	 * @param Name The name of the body modifier to access. 
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the skeletal
	 *        animation, rather than world space. Only relevant if the body is part of a skeletal mesh.
	 * @return true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierUseSkeletalAnimation(
		const FName Name,
		const bool  bUseSkeletalAnimation);

	/**
	 * Sets whether body modifiers should use skeletal animation for their kinematic targets
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *        then it can be split.
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the 
	 *        skeletal animation, rather than world space. Only relevant if the
	 *        body is part of a skeletal mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersUseSkeletalAnimation(
		const TArray<FName>& Names,
		const bool           bUseSkeletalAnimation);

	/**
	 * Sets whether body modifiers should use skeletal animation for their kinematic targets
	 *
	 * @param Set The set of body modifiers to modify. Standard sets will include "All" and things like
	 *        "ArmLeft", depending on how body modifiers have been created.
	 * @param bUseSkeletalAnimation Whether the kinematic target is specified in the frame of the
	 *        skeletal animation, rather than world space. Only relevant if the
	 *        body is part of a skeletal mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersInSetUseSkeletalAnimation(
		const FName Set,
		const bool  bUseSkeletalAnimation);

	/**
	 * Sets whether a body modifier should update kinematics from the simulation results
	 *
	 * @param Name The name of the body modifier to access. 
	 * @param bUpdateKinematicFromSimulation Whether the body should be updated from the simulation when
	 *        it is kinematic, or whether it should track the kinematic target directly. This will be most
	 *        likely useful when using async physics, in order to make kinematic parts behave the same as
	 *        dynamic ones.
	 * @return true if the body modifier was found, false if not
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetBodyModifierUpdateKinematicFromSimulation(
		const FName Name,
		const bool  bUpdateKinematicFromSimulation);

	/**
	 * Sets whether body modifiers should update kinematics from the simulation results
	 *
	 * @param Names The names of the body modifiers to access. Note that if you have these in a FPhysicsControlNameArray
	 *        then it can be split.
	 * @param bUpdateKinematicFromSimulation Whether the body should be updated from the simulation when
	 *        it is kinematic, or whether it should track the kinematic target directly. This will be most
	 *        likely useful when using async physics, in order to make kinematic parts behave the same as
	 *        dynamic ones.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersUpdateKinematicFromSimulation(
		const TArray<FName>& Names,
		const bool           bUpdateKinematicFromSimulation);

	/**
	 * Sets whether body modifiers should update kinematics from the simulation results
	 *
	 * @param Set The set of body modifiers to modify. Standard sets will include "All" and things like
	 *        "ArmLeft", depending on how body modifiers have been created.
	 * @param bUpdateKinematicFromSimulation Whether the body should be updated from the simulation when
	 *        it is kinematic, or whether it should track the kinematic target directly. This will be most
	 *        likely useful when using async physics, in order to make kinematic parts behave the same as
	 *        dynamic ones.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void SetBodyModifiersInSetUpdateKinematicFromSimulation(
		const FName Set,
		const bool  bUpdateKinematicFromSimulation);

	/**
	 * Creates a collections of controls and body modifiers for a character, based on the description passed in. 
	 * This makes:
	 * - World-space controls
	 * - Parent-space controls
	 * - Body modifiers
	 * for all the body parts. In addition, they get added to sets, so they can be referenced later. Each control 
	 * is added to three sets:
	 * - "All"
	 * - "ControlType - i.e. "WorldSpace" or "ParentSpace", each of which will end up containing all controls of that type
	 * - "ControlType_LimbName" - e.g. "WorldSpace_ArmLeft" or "ParentSpace_Head"
	 * Each body modifier is added to "All" and a set named after the limb - e.g. "Spine" or "LegRight".
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void CreateControlsAndBodyModifiersFromLimbBones(
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
		const EPhysicsMovementType                  PhysicsMovementType = EPhysicsMovementType::Static,
		const float                                 GravityMultiplier = 1.0f,
		const float                                 PhysicsBlendWeight = 1.0f
		);

	/**
	 * Adds a Control to a Set. This will add a new set if necessary. For example, you might
	 * make a set of Controls called "ParentSpace_Feet" by calling this twice, passing in the left and right
	 * foot ParentSpace controls.
	 * 
	 * @return The new/updated set of controls, in case you want to store it
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void AddControlToSet(FPhysicsControlNames& NewSet, const FName Control, const FName Set);

	/**
	 * Adds Controls to a Set. This will add a new set if necessary. For example, you might
	 * make a set of ParentSpace Arm controls by calling this twice, passing in the left and right
	 * arm ParentSpace controls.
	 * 
	 * @return The new/updated set of controls, in case you want to store it
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void AddControlsToSet(FPhysicsControlNames& NewSet, const TArray<FName>& Controls, const FName Set);

	/**
	 * Returns a reference to all the control names that have been created.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	const TArray<FName>& GetAllControlNames() const;

	/**
	 * Returns a reference to all the control names that have been created and are in the specified 
	 * set, which could be a limb, or a subsequently created set. Standard sets will include "All", "WorldSpace",
	 * "ParentSpace" and things like "WorldSpace-ArmLeft", depending on how controls have been created.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	const TArray<FName>& GetControlNamesInSet(const FName Set) const;

	/**
	 * Returns a reference to all the body modifier names that have been created.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	const TArray<FName>& GetAllBodyModifierNames() const;

	/**
	 * Returns a reference to all the body modifier names that have been created and are in the specified
	 * set, which could be a limb, or a subsequently created set. Standard sets will include "All" and things like
	 * "ArmLeft", depending on how body modifiers have been created.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	const TArray<FName>& GetBodyModifierNamesInSet(const FName Set) const;

	/**
	 * Adds a BodyModifier to a Set. This will add a new set if necessary. For example, you might
	 * make a set of body modifiers called "Feet" by calling this twice, passing in the left and right
	 * foot body modifiers.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void AddBodyModifierToSet(FPhysicsControlNames& NewSet, const FName BodyModifier, const FName Set);

	/**
	 * Adds BodyModifiers to a Set. This will add a new set if necessary. For example, you might
	 * make a set of Arm body modifiers by calling this twice, passing in the left and right
	 * arm body modifiers.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void AddBodyModifiersToSet(FPhysicsControlNames& NewSet, const TArray<FName>& BodyModifiers, const FName Set);

	/**
	 * Returns the names of all sets containing the control (may be empty - e.g. if it doesn't exist)
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> GetSetsContainingControl(const FName Control) const;

	/**
	 * Returns the names of all sets containing the body modifier (may be empty - e.g. if it doesn't exist)
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FName> GetSetsContainingBodyModifier(const FName Control) const;

	/**
	 * Gets the transforms of the requested bones that will be used as targets (in world space). Targets for bones 
	 * that are not found will be set to Identity. Note that these targets will have been calculated and cached 
	 * at the start of the Physics Control Component, so if using the built in tick, may be too old to be useful. 
	 * If you manually update the component then you can access these target transforms prior to applying your 
	 * own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FTransform> GetCachedBoneTransforms(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FName>&          BoneNames);

	/**
	 * Gets the positions of the requested bones that will be used as targets (in world space). Targets for bones 
	 * that are not found will be set to zero. Note that these targets will have been calculated and cached 
	 * at the start of the Physics Control Component, so if using the built in tick, may be too old to be useful. 
	 * If you manually update the component then you can access these target transforms prior to applying your 
	 * own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FVector> GetCachedBonePositions(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FName>&          BoneNames);

	/**
	 * Gets the orientations of the requested bones that will be used as targets (in world space). Targets for bones 
	 * that are not found will be set to identity. Note that these targets will have been calculated and cached 
	 * at the start of the Physics Control Component, so if using the built in tick, may be too old to be useful. 
	 * If you manually update the component then you can access these target transforms prior to applying your 
	 * own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FRotator> GetCachedBoneOrientations(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FName>&          BoneNames);

	/**
	 * Gets the linear velocities of the requested bones that will be used as targets (in world space). Target 
	 * velocities for bones that are not found will be set to zero. Note that these targets will have been 
	 * calculated and cached at the start of the Physics Control Component, so if using the built in tick, 
	 * may be too old to be useful. If you manually update the component then you can access these target 
	 * velocities prior to applying your own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FVector> GetCachedBoneVelocities(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FName>&          BoneNames);

	/**
	 * Gets the angular velocities of the requested bones that will be used as targets (in world space). Target 
	 * velocities for bones that are not found will be set to zero. Note that these targets will have been 
	 * calculated and cached at the start of the Physics Control Component, so if using the built in tick, 
	 * may be too old to be useful. If you manually update the component then you can access these target 
	 * velocities prior to applying your own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	TArray<FVector> GetCachedBoneAngularVelocities(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const TArray<FName>&          BoneNames);

	/**
	 * Gets the transforms of the requested bone that will be used as a target (in world space). Targets for bones
	 * that are not found will be set to Identity. Note that these targets will have been calculated and cached
	 * at the start of the Physics Control Component, so if using the built in tick, may be too old to be useful.
	 * If you manually update the component then you can access these target transforms prior to applying your
	 * own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FTransform GetCachedBoneTransform(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const FName                   BoneName);

	/**
	 * Gets the position of the requested bone that will be used as a target (in world space). Targets for bones
	 * that are not found will be set to zero. Note that these targets will have been calculated and cached
	 * at the start of the Physics Control Component, so if using the built in tick, may be too old to be useful.
	 * If you manually update the component then you can access these target transforms prior to applying your
	 * own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FVector GetCachedBonePosition(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const FName                   BoneName);

	/**
	 * Gets the orientation of the requested bone that will be used as a target (in world space). Targets for bones
	 * that are not found will be set to Identity. Note that these targets will have been calculated and cached
	 * at the start of the Physics Control Component, so if using the built in tick, may be too old to be useful.
	 * If you manually update the component then you can access these target transforms prior to applying your
	 * own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FRotator GetCachedBoneOrientation(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const FName                   BoneName);

	/**
	 * Gets the linear velocity of the requested bone that will be used as a target (in world space). Target 
	 * velocities for bones that are not found will be set to zero. Note that these targets will have been 
	 * calculated and cached at the start of the Physics Control Component, so if using the built in tick, 
	 * may be too old to be useful. If you manually update the component then you can access these target 
	 * velocities prior to applying your own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FVector GetCachedBoneVelocity(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const FName                   BoneName);

	/**
	 * Gets the angular velocity of the requested bone that will be used as a target (in world space). Target 
	 * velocities for bones that are not found will be set to zero. Note that these targets will have been 
	 * calculated and cached at the start of the Physics Control Component update, so if using the built in tick, 
	 * may be too old to be useful. If you manually update the component then you can access these target 
	 * velocities prior to applying your own targets.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	FVector GetCachedBoneAngularVelocity(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const FName                   BoneName);

	/**
	 * This allows the caller to override the target that will have been calculated and cached at the start of 
	 * the Physics Control Component update. This is unlikely to be useful when using the built in tick, 
	 * but if you are manually updating the component then you may wish to call this after UpdateTargetCaches 
	 * but before UpdateControls. 
	 * 
	 * @return true if successful, and false if no cached target can be found for the bone.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool SetCachedBoneData(
		const USkeletalMeshComponent* SkeletalMeshComponent,
		const FName                   BoneName, 
		const FTransform&             TM,
		const FVector                 Velocity,
		const FVector                 AngularVelocity);

	/**
	 * This flags the body associated with the modifier to set (using teleport) its position and velocity to 
	 * the cached animation target. This will only affect skeletal mesh component bodies. 
	 * 
	 * @param Name     The name of the body modifier to use to identify the body to reset.
	 * @param Behavior When the reset should happen.
	 * 
	 * @return true if the body modifier is found (even if no cached target is found), and false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	bool ResetBodyModifierToCachedBoneTransform(
		const FName                        Name,
		const EResetToCachedTargetBehavior Behavior = EResetToCachedTargetBehavior::ResetImmediately);

	/**
	 * Calls ResetBodyModifierToCachedTarget for each of the body modifiers
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void ResetBodyModifiersToCachedBoneTransforms(
		const TArray<FName>&               Names,
		const EResetToCachedTargetBehavior Behavior = EResetToCachedTargetBehavior::ResetImmediately);

	/**
	 * Calls ResetBodyModifierToCachedTarget for each of the body modifiers in the set
	 */
	UFUNCTION(BlueprintCallable, Category = PhysicsControl)
	void ResetBodyModifiersInSetToCachedBoneTransforms(
		const FName                        SetName,
		const EResetToCachedTargetBehavior Behavior = EResetToCachedTargetBehavior::ResetImmediately);

public:

	/**
	 * If the component moves by more than this distance then it is treated as a teleport,
	 * which prevents velocities being used for a frame. It is also used as the threshold for
	 * teleporting when moving kinematic objects. Zero or negative disables.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Teleport)
	float TeleportDistanceThreshold = 300.0f;

	/**
	 * If the component rotates by more than this angle (in degrees) then it is treated as a teleport,
	 * which prevents velocities being used for a frame. It is also used as the threshold for
	 * teleporting when moving kinematic objects. Zero or negative disables.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Teleport)
	float TeleportRotationThreshold = 0.0f;

	/** Visualize the controls when this component is selected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	bool bShowDebugVisualization = true;

	/** Size of the gizmos etc used during visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	float VisualizationSizeScale = 5.0f;

	/** Display all the controls and their basic properties when this component is selected*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	bool bShowDebugControlList = false;

	/** Display detailed info for controls containing this string (if non-empty) when this component is selected*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	FString DebugControlDetailFilter;

	/** Display all the body modifiers and their basic properties when this component is selected*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	bool bShowDebugBodyModifierList = false;

	/** Display detailed info for body modifiers containing this string (if non-empty) when this component is selected*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	FString DebugBodyModifierDetailFilter;

	/**
	 * The time used when "predicting" the target position/orientation. Zero will disable the visualization
	 * of this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	float VelocityPredictionTime = 0.2f;

	/**
	 * Upper limit on the number of controls or modifiers that will be created using the same name (which
	 * will get a numerical postfix). When this limit is reached a warning will be issued  and the control 
	 * or modifier won't be created. This is to avoid problems if controls or modifiers are being created 
	 * dynamically, and can generally be a "moderately large" number, depending on how many controls or 
	 * modifiers you expect to create.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	int32 MaxNumControlsOrModifiersPerName = 256;

protected:

	// Begin UActorComponent Interface
	virtual void InitializeComponent() override;
	virtual void BeginDestroy() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// End UActorComponent Interface

public:
#if WITH_EDITOR
	//Begin ActorComponent interface
	virtual void OnRegister() override;
	//End ActorComponent interface

	void UpdateSpriteTexture();
	// Used by the component visualizer
	void DebugDraw(FPrimitiveDrawInterface* PDI) const;
	void DebugDrawControl(FPrimitiveDrawInterface* PDI, const FPhysicsControlRecord& Record, const FName ControlName) const;
#endif

protected:

	TPimplPtr<FPhysicsControlComponentImpl> Implementation;
};
