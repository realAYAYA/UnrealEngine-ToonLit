// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineBaseTypes.h"
#include "Framework/Commands/InputChord.h"
#include "K2Node_Event.h"
#include "K2Node_InputDebugKeyEvent.generated.h"

class UDynamicBlueprintBinding;

UCLASS()
class INPUTBLUEPRINTNODES_API UK2Node_InputDebugKeyEvent : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FInputChord InputChord;

	UPROPERTY()
	TEnumAsByte<EInputEvent> InputKeyEvent;

	UPROPERTY()
	bool bExecuteWhenPaused = false;

	//~ Begin UK2Node Interface
	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const override;
	//~ End UK2Node Interface
};
