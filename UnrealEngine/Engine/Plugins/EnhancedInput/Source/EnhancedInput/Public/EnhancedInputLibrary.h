// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingQuery.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/ObjectMacros.h"

#include "EnhancedInputLibrary.generated.h"

class APlayerController;
class UInputMappingContext;
class UInputAction;
class UEnhancedPlayerInput;

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
};
