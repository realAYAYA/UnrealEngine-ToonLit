// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/InputDelegateBinding.h"
#include "Framework/Commands/InputChord.h"
#include "InputDebugKeyDelegateBinding.generated.h"

class UInputComponent;

USTRUCT()
struct ENHANCEDINPUT_API FBlueprintInputDebugKeyDelegateBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FInputChord InputChord;

	UPROPERTY()
	TEnumAsByte<EInputEvent> InputKeyEvent = IE_Pressed;

	UPROPERTY()
	FName FunctionNameToBind = NAME_None;

	UPROPERTY()
	bool bExecuteWhenPaused = false;

	// TODO: bConsumeInput?
};

UCLASS()
class ENHANCEDINPUT_API UInputDebugKeyDelegateBinding : public UInputDelegateBinding
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FBlueprintInputDebugKeyDelegateBinding> InputDebugKeyDelegateBindings;

	//~ Begin UInputDelegateBinding Interface
	virtual void BindToInputComponent(UInputComponent* InputComponent, UObject* ObjectToBindTo) const override;
	//~ End UInputDelegateBinding Interface
};
