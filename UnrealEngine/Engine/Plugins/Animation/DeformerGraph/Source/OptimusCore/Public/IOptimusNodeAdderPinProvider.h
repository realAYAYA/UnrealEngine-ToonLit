// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "OptimusNode.h"

#include "IOptimusNodeAdderPinProvider.generated.h"

UINTERFACE()
class OPTIMUSCORE_API UOptimusNodeAdderPinProvider :
	public UInterface
{
	GENERATED_BODY()
};

/**
* Interface that provides a mechanism to add pins to node from existing pins
*/
class OPTIMUSCORE_API IOptimusNodeAdderPinProvider
{
	GENERATED_BODY()

public:
	struct FAdderPinAction
	{
		FName DisplayName;
		EOptimusNodePinDirection NewPinDirection;
		FName Key;
		FText ToolTip;
		bool bCanAutoLink;
	};
	
	virtual TArray<FAdderPinAction> GetAvailableAdderPinActions(
		const UOptimusNodePin* InSourcePin,
		EOptimusNodePinDirection InNewPinDirection,
		FString* OutReason = nullptr
		) const = 0;
	
	virtual TArray<UOptimusNodePin*> TryAddPinFromPin(
		const FAdderPinAction& InSelectedAction,
		UOptimusNodePin* InSourcePin
		) = 0;
	
	virtual bool RemoveAddedPins(TConstArrayView<UOptimusNodePin*> InAddedPinsToRemove) = 0;
};
