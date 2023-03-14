// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineBaseTypes.h"
#include "EnhancedPlayerInput.h"
#include "K2Node_Event.h"
#include "K2Node_EnhancedInputActionEvent.generated.h"

class UDynamicBlueprintBinding;

UCLASS()
class INPUTBLUEPRINTNODES_API UK2Node_EnhancedInputActionEvent : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TObjectPtr<const UInputAction> InputAction;

	UPROPERTY()
	ETriggerEvent TriggerEvent;

	//~ Begin UK2Node Interface
	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const override;
	//~ End UK2Node Interface
};
