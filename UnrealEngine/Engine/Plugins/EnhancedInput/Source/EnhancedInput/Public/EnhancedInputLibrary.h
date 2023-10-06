// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "PlayerMappableKeySlot.h"

#include "EnhancedInputLibrary.generated.h"

class IEnhancedInputSubsystemInterface;
enum class EInputActionValueType : uint8;
struct FEnhancedActionKeyMapping;
struct FInputActionValue;

class APlayerController;
class UEnhancedPlayerInput;
class UInputAction;
class UInputMappingContext;
class UPlayerMappableKeySettings;

UCLASS()
class ENHANCEDINPUT_API UEnhancedInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Call SubsystemPredicate on each registered player and standalone enhanced input subsystem.
	 */
	static void ForEachSubsystem(TFunctionRef<void(IEnhancedInputSubsystemInterface*)> SubsystemPredicate);

	/**
	 * Flag all enhanced input subsystems making use of the mapping context for reapplication of all control mappings at the end of this frame.
	 * @param Context				Mappings will be rebuilt for all subsystems utilizing this context.
	 * @param bForceImmediately		The mapping changes will be applied synchronously, rather than at the end of the frame, making them available to the input system on the same frame.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input")
	static void RequestRebuildControlMappingsUsingContext(const UInputMappingContext* Context, bool bForceImmediately = false);


	/** Breaks an ActionValue into X, Y, Z. Axes not supported by value type will be 0. */
	UFUNCTION(BlueprintPure, Category = "Input", meta = (NativeBreakFunc))
	static void BreakInputActionValue(FInputActionValue InActionValue, double& X, double& Y, double& Z, EInputActionValueType& Type);

	/**
	 * Builds an ActionValue from X, Y, Z. Inherits type from an existing ActionValue. Ignores axis values unused by the provided value type.
	 * @note Intended for use in Input Modifier Modify Raw overloads to modify an existing Input Action Value.
	 */
	UFUNCTION(BlueprintPure, Category = "Input", meta = (Keywords = "construct build", NativeMakeFunc))
	static FInputActionValue MakeInputActionValueOfType(double X, double Y, double Z, const EInputActionValueType ValueType);

	UE_DEPRECATED(5.1, "This version of MakeInputActionValue has been deprecated, please use MakeInputActionValueOfType")
	UFUNCTION(BlueprintPure, Category = "Input", meta = (Keywords = "construct build", NativeMakeFunc, DeprecatedFunction, DeprecatedMessage="This version of MakeInputActionValue has been deprecated, please use MakeInputActionValueOfType"))
	static FInputActionValue MakeInputActionValue(double X, double Y, double Z, const FInputActionValue& MatchValueType);

	/**
	* Returns the Player Mappable Key Settings owned by the Action Key Mapping or by the referenced Input Action, or nothing based of the Setting Behavior.
	*/
	UFUNCTION(BlueprintPure, Category = "Input", meta = (ReturnDisplayName = "Player Mappable Key Settings"))
	static UPlayerMappableKeySettings* GetPlayerMappableKeySettings(UPARAM(ref) const FEnhancedActionKeyMapping& ActionKeyMapping);

	/**
	* Returns the name of the mapping based on setting behavior used. If no name is found in the Mappable Key Settings it will return the name set in Player Mappable Options if bIsPlayerMappable is true.
	*/
	UFUNCTION(BlueprintPure, Category = "Input", meta = (ReturnDisplayName = "Mapping Name"))
	static FName GetMappingName(UPARAM(ref) const FEnhancedActionKeyMapping& ActionKeyMapping);

	/**
	 * Returns true if this Action Key Mapping either holds a Player Mappable Key Settings or is set bIsPlayerMappable.
	 */
	UFUNCTION(BlueprintPure, Category = "Input", meta = (ReturnDisplayName = "Is Player Mappable"))
	static bool IsActionKeyMappingPlayerMappable(UPARAM(ref) const FEnhancedActionKeyMapping& ActionKeyMapping);

	// Internal helper functionality

	// GetInputActionvalue internal accessor function for actions that have been bound to from a UEnhancedInputComponent
	UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, meta = (HidePin = "Action"))
	static FInputActionValue GetBoundActionValue(AActor* Actor, const UInputAction* Action);

	// FInputActionValue internal auto-converters.

	/** Interpret an InputActionValue as a boolean input */
	UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, meta = (BlueprintAutocast))
	static bool Conv_InputActionValueToBool(FInputActionValue InValue);

	/** Interpret an InputActionValue as a 1D axis (double) input */
	UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, meta = (BlueprintAutocast))
	static double Conv_InputActionValueToAxis1D(FInputActionValue InValue);

	/** Interpret an InputActionValue as a 2D axis (Vector2D) input */
	UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, meta = (BlueprintAutocast))
	static FVector2D Conv_InputActionValueToAxis2D(FInputActionValue InValue);

	/** Interpret an InputActionValue as a 3D axis (Vector) input */
	UFUNCTION(BlueprintPure, BlueprintInternalUseOnly, meta = (BlueprintAutocast))
	static FVector Conv_InputActionValueToAxis3D(FInputActionValue ActionValue);

	/** Converts a FInputActionValue to a string */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To String (InputActionValue)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|String")
	static FString Conv_InputActionValueToString(FInputActionValue ActionValue);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.3, "FPlayerMappableKeySlot has been deprecated. Please use EPlayerMappableKeySlot instead.")
	UFUNCTION(BlueprintPure, Category = "Input", meta = (ReturnDisplayName = "First Player Mappable Key Slot", DeprecatedFunction, DeprecationMessage="FPlayerMappableKeyOptions has been deprecated. Please use UPlayerMappableKeySettings instead."))
	static FPlayerMappableKeySlot& GetFirstPlayerMappableKeySlot() { return FPlayerMappableKeySlot::FirstKeySlot; };

	UE_DEPRECATED(5.3, "FPlayerMappableKeySlot has been deprecated. Please use EPlayerMappableKeySlot instead.")
	UFUNCTION(BlueprintPure, Category = "Input", meta = (ReturnDisplayName = "Second Player Mappable Key Slot", DeprecatedFunction, DeprecationMessage="FPlayerMappableKeyOptions has been deprecated. Please use UPlayerMappableKeySettings instead."))
	static FPlayerMappableKeySlot& GetSecondPlayerMappableKeySlot() { return FPlayerMappableKeySlot::SecondKeySlot; };

	UE_DEPRECATED(5.3, "FPlayerMappableKeySlot has been deprecated. Please use EPlayerMappableKeySlot instead.")
	UFUNCTION(BlueprintPure, Category = "Input", meta = (ReturnDisplayName = "Third Player Mappable Key Slot", DeprecatedFunction, DeprecationMessage="FPlayerMappableKeyOptions has been deprecated. Please use UPlayerMappableKeySettings instead."))
	static FPlayerMappableKeySlot& GetThirdPlayerMappableKeySlot() { return FPlayerMappableKeySlot::ThirdKeySlot; };

	UE_DEPRECATED(5.3, "FPlayerMappableKeySlot has been deprecated. Please use EPlayerMappableKeySlot instead.")
	UFUNCTION(BlueprintPure, Category = "Input", meta = (ReturnDisplayName = "Fourth Player Mappable Key Slot", DeprecatedFunction, DeprecationMessage="FPlayerMappableKeyOptions has been deprecated. Please use UPlayerMappableKeySettings instead."))
	static FPlayerMappableKeySlot& GetFourthPlayerMappableKeySlot() { return FPlayerMappableKeySlot::FourthKeySlot; };
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingQuery.h"
#endif
