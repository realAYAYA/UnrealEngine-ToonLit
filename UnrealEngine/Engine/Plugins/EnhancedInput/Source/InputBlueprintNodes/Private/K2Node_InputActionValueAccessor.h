// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineBaseTypes.h"
#include "InputAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputActionValueAccessor.generated.h"

UCLASS()
class INPUTBLUEPRINTNODES_API UK2Node_InputActionValueAccessor : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin EdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	//~ End EdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(class UDynamicBlueprintBinding* BindingObject) const override;
	virtual bool IsNodePure() const override { return true; }
	//~ End UK2Node Interface

	void Initialize(const UInputAction* Action);

private:
	UPROPERTY()
	TObjectPtr<const UInputAction> InputAction;
};
