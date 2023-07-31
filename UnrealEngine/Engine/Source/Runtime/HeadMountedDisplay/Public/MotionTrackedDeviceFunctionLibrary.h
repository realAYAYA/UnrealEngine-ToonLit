// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MotionControllerComponent.h"
#include "MotionTrackedDeviceFunctionLibrary.generated.h"

class IMotionController;
enum class EControllerHand : uint8;

UCLASS()
class HEADMOUNTEDDISPLAY_API UMotionTrackedDeviceFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/**
	* Returns true if it is necessary for the game to manage how many motion tracked devices it is asking to be tracked simultaneously.
	* On some platforms this is unnecessary because all supported devices can be tracked simultaneously.
	*
	* @return (Boolean) true if the game might need to manage which motion tracked devices are actively tracked.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|MotionTracking")
	static bool IsMotionTrackedDeviceCountManagementNecessary();

	/**
	* Set whether motion tracked controllers activate on creation by default, or do not and must be explicitly activated.
	*
	* @param Require					(in) True means controllers are enabled by default.  Enables beyond the controller count limit will fail.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static void SetIsControllerMotionTrackingEnabledByDefault(bool Enable);

	/**
	* Get the maximum number of controllers that can be tracked.
	*
	* @return (int) number of controllers that can be tracked, or -1 if there is no limit (IsMotionTrackedDeviceCountManagementNecessary() should return false).
	*/
	UFUNCTION(BlueprintPure, Category = "Input|MotionTracking")
	static int32 GetMaximumMotionTrackedControllerCount();

	/**
	* Get the number of controllers for which tracking is enabled.
	*
	* @return (int) number of controllers tracked now, or -1 if this query is unsupported (IsMotionTrackedDeviceCountManagementNecessary() should return false).
	*/
	UFUNCTION(BlueprintPure, Category = "Input|MotionTracking")
	static int32 GetMotionTrackingEnabledControllerCount();

	/**
	 * Returns true if tracking is enabled for the specified device.
	 *
	 * @param PlayerIndex					(in) The index of the player.
	 * @param Hand							(in) The tracked device type.
	 *
	 * @return (Boolean) true if the specified device is set to be tracked.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|MotionTracking", meta = (DeprecatedFunction, DeprecationMessage = "EControllerHand has been deprecated, please use IsMotionTrackingEnabledForSource instead."))
	static bool IsMotionTrackingEnabledForDevice(int32 PlayerIndex, EControllerHand Hand);

	/**
	 * Returns true if tracking is enabled for the specified device.
	 *
	 * @param PlayerIndex					(in) The index of the player.
	 * @param SourceName					(in) The tracked device id.
	 *
	 * @return (Boolean) true if the specified device is set to be tracked.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|MotionTracking")
	static bool IsMotionTrackingEnabledForSource(int32 PlayerIndex, FName SourceName);

	/**
	* Returns true if tracking is enabled for the specified device.
	*
	* @param MotionControllerComponent		(in) The motion controller component who's associated device is targeted.
	*
	* @return (Boolean) true if the specified device is set to be tracked.
	*/
	UFUNCTION(BlueprintPure, Category = "Input|MotionTracking")
	static bool IsMotionTrackingEnabledForComponent(const class UMotionControllerComponent* MotionControllerComponent);

	/**
	 * Enable tracking of the specified controller, by player index and tracked device type.
	 *
	 * @param PlayerIndex					(in) The index of the player.
	 * @param Hand							(in) The device type.
	 *
	 * @return (Boolean) true if the specified device is now set to be tracked.  This could fail due to tracking limits, or on invalid input.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking", meta = (DeprecatedFunction, DeprecationMessage = "EControllerHand has been deprecated, please use EnableMotionTrackingOfSource instead."))
	static bool EnableMotionTrackingOfDevice(int32 PlayerIndex, EControllerHand Hand);

	/**
	 * Enable tracking of the specified controller, by player index and tracked device type.
	 *
	 * @param PlayerIndex					(in) The index of the player.
	 * @param SourceName					(in) The device id.
	 *
	 * @return (Boolean) true if the specified device is now set to be tracked.  This could fail due to tracking limits, or on invalid input.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static bool EnableMotionTrackingOfSource(int32 PlayerIndex, FName SourceName);

	/**
	* Enable tracking of the specified controller, by player index and tracked device type.
	*
	* @param MotionControllerComponent		(in) The motion controller component who's associated device is targeted.
	*
	* @return (Boolean) true if the specified device is now set to be tracked.  This could fail due to tracking limits, or on invalid input.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static bool EnableMotionTrackingForComponent(class UMotionControllerComponent* MotionControllerComponent);

	/**
	 * Disable tracking of the specified controller, by player index and tracked device type.
	 *
	 * @param PlayerIndex					(in) The index of the player.
	 * @param Hand							(in) The tracked device type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking", meta = (DeprecatedFunction, DeprecationMessage = "EControllerHand has been deprecated, please use DisableMotionTrackingOfSource instead."))
	static void DisableMotionTrackingOfDevice(int32 PlayerIndex, EControllerHand Hand);

	/**
	 * Disable tracking of the specified controller, by player index and tracked device type.
	 *
	 * @param PlayerIndex					(in) The index of the player.
	 * @param SourceName					(in) The tracked device id.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static void DisableMotionTrackingOfSource(int32 PlayerIndex, FName SourceName);

	/**
	* Disable tracking of the specified controller, by player index and tracked device type.
	*
	* @param MotionControllerComponent		(in) The motion controller component who's associated device is targeted.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static void DisableMotionTrackingForComponent(const class UMotionControllerComponent* MotionControllerComponent);

	/**
	* Disable tracking for all controllers.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static void DisableMotionTrackingOfAllControllers();

	/**
	* Disable tracking for all controllers associated with the specified player.
	*
	* @param PlayerIndex					(in) The index of the player.
	*/
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static void DisableMotionTrackingOfControllersForPlayer(int32 PlayerIndex);

	/**
	 * Returns a list of all available motion sources (FNames associated with 
	 * discrete tracking data that can be used to drive MotionControllerComponents).
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static TArray<FName> EnumerateMotionSources();

	/**
	 * Returns the system name used to distinguish the current tracking system.
	 * If no XR tracking system is active, then the name 'None' is returned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static FName GetActiveTrackingSystemName();

	/**
	 * Queries the specified source's tracking status and returns true if it has tracking.
	 *
	 * @return Tracking status of the specified controller.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input|MotionTracking")
	static bool IsMotionSourceTracking(int32 PlayerIndex, FName SourceName);
};
