// Copyright Epic Games, Inc. All Rights Reserved.

#include "K2Node_InputTouchEvent.h"

#include "Containers/Array.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "Engine/InputTouchDelegateBinding.h"
#include "HAL/PlatformCrt.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"

class UClass;

UK2Node_InputTouchEvent::UK2Node_InputTouchEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bConsumeInput = true;
	bOverrideParentBinding = true;
	bInternalEvent = true;
}

UClass* UK2Node_InputTouchEvent::GetDynamicBindingClass() const
{
	return UInputTouchDelegateBinding::StaticClass();
}

void UK2Node_InputTouchEvent::RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const
{
	UInputTouchDelegateBinding* InputTouchBindingObject = CastChecked<UInputTouchDelegateBinding>(BindingObject);

	FBlueprintInputTouchDelegateBinding Binding;
	Binding.InputKeyEvent = InputKeyEvent;
	Binding.bConsumeInput = bConsumeInput;
	Binding.bExecuteWhenPaused = bExecuteWhenPaused;
	Binding.bOverrideParentBinding = bOverrideParentBinding;
	Binding.FunctionNameToBind = CustomFunctionName;

	InputTouchBindingObject->InputTouchDelegateBindings.Add(Binding);
}
