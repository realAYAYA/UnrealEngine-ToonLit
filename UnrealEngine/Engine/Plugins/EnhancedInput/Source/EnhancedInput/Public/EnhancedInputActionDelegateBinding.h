// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/InputDelegateBinding.h"
#include "InputTriggers.h"
#include "EnhancedInputActionDelegateBinding.generated.h"

class UInputComponent;

USTRUCT()
struct ENHANCEDINPUT_API FBlueprintEnhancedInputActionBinding
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY()
	ETriggerEvent TriggerEvent = ETriggerEvent::None;

	UPROPERTY()
	FName FunctionNameToBind = NAME_None;

	// TODO: bDevelopmentOnly;	// This action delegate will not fire in shipped builds (debug/cheat actions)
};



UCLASS()
class ENHANCEDINPUT_API UEnhancedInputActionDelegateBinding : public UInputDelegateBinding
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FBlueprintEnhancedInputActionBinding> InputActionDelegateBindings;

	//~ Begin UInputDelegateBinding Interface
	virtual void BindToInputComponent(UInputComponent* InputComponent, UObject* ObjectToBindTo) const override;
	//~ End UInputDelegateBinding Interface
};

UCLASS()
class ENHANCEDINPUT_API UEnhancedInputActionValueBinding : public UInputDelegateBinding
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FBlueprintEnhancedInputActionBinding> InputActionValueBindings;

	//~ Begin UInputDelegateBinding Interface
	virtual void BindToInputComponent(UInputComponent* InputComponent, UObject* ObjectToBindTo) const override;
	//~ End UInputDelegateBinding Interface
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "EnhancedPlayerInput.h"
#endif
