// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PawnMovementComponent.h"
#include "VirtualCameraSaveGame.h"
#include "VirtualCameraMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVirtualCameraResetOffsetsDelegate);

UCLASS()
class VIRTUALCAMERA_API UVirtualCameraMovementComponent : public UPawnMovementComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** Stores all settings for each axis */
	TMap<EVirtualCameraAxis, FVirtualCameraAxisSettings> AxisSettings;

	/** Tracks whether user wants to cache a locking axis set or always use local */
	bool bLockRelativeToFirstLockAxis;

	/** Tracks if boom should be global or relative to the camera */
	bool bUseGlobalBoom;

	/** Tracks if dutch should be reset to zero when freezing view */
	bool bZeroDutchOnLock;

	/** Delegate to broadcast when tracker offsets are reset */
	FVirtualCameraResetOffsetsDelegate OnOffsetReset;

	/**
	 * Adds the given vector to the accumulated input in world space. Input vectors are usually between 0 and 1 in magnitude.
	 * They are accumulated during a frame then applied as acceleration during the movement update.
	 * @param WorldDirection - Direction in world space to apply input
	 * @param bForce - If true always add the input, ignoring the result of IsMoveInputIgnored().
	 * @see APawn::AddMovementInput()
	 */
	virtual void AddInputVector(FVector WorldVector, bool bForce = false) override;

	/**
	 * Adds the given vector to the accumulated input in world space. Similar to AddInputVector().
	 * This calls input scaling specific to how controller (joystick) input should be handled.
	 * @param WorldVector - Direction in world space to apply input.
	 * @param MovementScaleAxis - Which axis movement scale to use.
	 */
	void AddInputVectorFromController(FVector WorldVector, EVirtualCameraAxis MovementScaleAxis);

	/**
	 * Update the position and rotation of the camera.
	 * @param Location - The current location of the tracker being used for input
	 * @param Rotation - The current rotation of the tracker being used for input
	 */
	void ProcessMovementInput(const FVector& Location, const FRotator& Rotation);

	/**
	 * Toggles the lock on a given axis; returns new locked state.
	 * @param AxisToToggle - The axis whose lock should be turned on/off
	 * @return the new locked state of AxisToToggle (true = locked)
	 */
	bool ToggleAxisLock(const EVirtualCameraAxis AxisToToggle);

	/**
	 * Toggles the freeze on a given axis; returns new frozen state.
	 * @param AxisToToggle - The axis whose lock should be toggled
	 * @return the new frozen state of AxisToToggle (true = frozen)
	 */
	bool ToggleAxisFreeze(const EVirtualCameraAxis AxisToToggle);

	/**
	 * Sets the stabilization rate for a given lock.
	 * @param AxisToAdjust - The axis whose stabilization rate should be changed
	 * @param NewStabilizationAmount - The stabilization amount we should attempt to set the value
	 * @return the actual value the stabilization amount was set to after clamping
	 */
	float SetAxisStabilizationScale(const EVirtualCameraAxis AxisToAdjust, float NewStabilizationAmount);

	/**
	 * Gets the stabilization rate for a given axis. 
	 * This method returns the stabilization value after undoing the curve applied in SetAxisStabiliaztionScale().
	 * The return value will be equivialent the the NewStabiliaztionAmount parameter when setting the value, and not the returned actual value.
	 * @param AxisToRetrieve - The axis whose stabilization rate is being retrieved
	 * @return the value of the stabilization amount for the current axis
	 */
	float GetAxisStabilizationScale(const EVirtualCameraAxis AxisToRetrieve) const;

	/**
	 * Gets the movement scale for a given axis.
	 * @param AxisToRetrieve - The axis whose movement scale is being retrieved
	 * @return the value of the movement scale for the current axis
	 */
	float GetAxisMovementScale(const EVirtualCameraAxis AxisToRetrieve) const { return AxisSettings[AxisToRetrieve].MovementScale; };

	/**
	 * Moves the camera back to actor root and aligns rotation with the input tracker.
	 */
	void ResetCameraOffsetsToTracker();

	/**
	 * Sets the movement scale of the camera actor
	 * @param AxisToAdjust - The axis to set movement scale on (setting for rotation axes is allowed but has no effect)
	 * @param NewMovementScale - The desired scaling factor for calculating movement along this axis
	 */
	void SetMovementScale(const EVirtualCameraAxis AxisToAdjust, const float NewMovementScale) { AxisSettings[AxisToAdjust].MovementScale = NewMovementScale; }

	/**
	 * Gets the movement scale of the camera actor
	 * @param AxisToRetrieve - The axis to get movement scale from (setting for rotation axes is allowed but has no effect)
	 * @return the scaling factor for calculating movement along this axis
	 */
	float GetMovementScale(const EVirtualCameraAxis AxisToRetrieve) const { return AxisSettings[AxisToRetrieve].MovementScale; }

	/**
	 * Check to see if any location locks are active
	 * @return true if any of the three location axes are locked
	 */
	bool IsLocationLockingActive() const;

	/**
	 * Checks if an axis is locked.
	 * @param AxisToCheck - The axis being checked.
	 * @return true if locked false otherwise
	 */
	bool IsAxisLocked(const EVirtualCameraAxis AxisToCheck) const;

	/**
	 * Set the value for the option to zero out dutch when locking that axis.
	 * @param bInValue - The new setting to use for zeroing dutch on lock
	 */
	void SetZeroDutchOnLock(const bool bInValue) { bZeroDutchOnLock = bInValue; }

	/**
	 * Get the value for the option to zero out dutch when locking that axis.
	 * @return true if dutch will be zeroed when that axis is locked
	 */
	bool IsDutchZeroingOnLockActive() const { return bZeroDutchOnLock; }

	/** 
	 * Handle a move forward command from the owner.
	 * @param InValue - The driving axis value for movement
	 */
	void OnMoveForward(const float InValue);

	/**
	 * Handle a move right command from the owner.
	 * @param InValue - The driving axis value for movement
	 */
	void OnMoveRight(const float InValue);

	/**
	 * Handle a move up command from the owner.
	 * @param InValue - The driving axis value for movement
	 */
	void OnMoveUp(const float InValue);

	/**
	 * Teleport to a provided location and rotation. 
	 * @param TargetTransform - The information for the target to teleport to
	 */
	void Teleport(const FTransform& TargetTransform);

	/**
	 * Get whether or not global boom is being used when navigating with the joysticks
	 * @param bShouldUseGlobalBoom - True if using global Z axis, false if using local Z axis
	 */
	bool IsUsingGlobalBoom() const { return bUseGlobalBoom; }

	/**
	 * Sets whether or not global boom should be used when navigating with the joysticks
	 * @param bShouldUseGlobalBoom - True to use global Z axis, false to use local Z axis
	 */
	void SetUseGlobalBoom(bool bShouldUseGlobalBoom) { bUseGlobalBoom = bShouldUseGlobalBoom; }


	/**
	 * When the input come from a physical controller, move this component instead.
	 * This allow a proper replication when we are in a multi user setting.
	 * This component doesn't respect the freeze and lock axis.
	 */
	void SetRootComponent(USceneComponent* FromController);

private:
	/** The cached axis for locking, only used if bLockRelativeToFirstLockAxis is true */
	FQuat CachedLockingAxis;

	/** Tracks the previous tracker location to determine which direction we should move */
	FVector PreviousTrackerLocation;

	/** Tracks the current target location of the camera for stabilization */
	FVector TargetLocation;

	/** Tracks the current target location of the camera that came from a controller */
	FVector FromControllerTargetLocation;

	/** Tracks the previous tracker rotation to determine how much we should rotate */
	FRotator PreviousTrackerRotation;

	/** Tracks the current target rotation of the camera for stabilization */
	FRotator TargetRotation;

	/**
	 * The component we move and update when the input is coming from the controller.
	 * If this is null UpdatedComponent will be used instead.
	 * @see UpdatedComponent
	 */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<USceneComponent> RootUpdatedComponent;

	/**
	 * RootUpdatedComponent, cast as a UPrimitiveComponent. May be invalid if RootUpdatedComponent was null or not a UPrimitiveComponent.
	 */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UPrimitiveComponent> RootUpdatedPrimitive;

	/**
	 * Applies relative axis filtering based on locks for location.
	 * @param InVector - The input vector that will be used to apply locking offsets
	 */
	void ApplyLocationLocks(const FVector& InVector);

	/**
	 * Get the position filtering vector, which is used to filter each axis independently.
	 * @return filtered location based on stabilization values
	 */
	FVector GetStabilizedDeltaLocation() const;

	/**
	 * Applies relative axis scaling for location.
	 * @param VectorToAdjust - Upon return, has input values with local space movement scaled by movement scale
	 */
	void ApplyLocationScaling(FVector& VectorToAdjust);

	/**
	 * Applies global axis filtering based on locks for rotation.
	 * @param InRotation - The input rotation that will be used to apply rotation offsets
	 */
	void ApplyRotationLocks(const FRotator& InRotation);

	/**
	 * Get the position filtering vector, which is used to filter each axis independently.
	 * @return filtered rotation
	 */
	FRotator GetStabilizedRotation() const;

	/**
	 * Get the directional vectors for movement controls, taking into account any cached axes as needed.
	 * @param OutForwardVector - Upon return, will hold the forward vector
	 * @param OutRightVector - Upon return, will hold the right vector
	 * @param OutUpVector - Upon return, will hold the up vector
	 * @param bAllowLock - Use the locking axis (if valid) or the camera axis
	 */
	void GetDirectionVectorsForCamera(FVector& OutForwardVector, FVector& OutRightVector, FVector& OutUpVector, bool bTryLock) const;

	/**
	 * Gets the current location offset
	 * @return - The current location offset
	 */
	FVector GetLocationOffset() const;

	/**
	 * Gets the current rotation offset
	 * @return - The current rotation offset
	 */
	FRotator GetRotationOffset() const;
};
