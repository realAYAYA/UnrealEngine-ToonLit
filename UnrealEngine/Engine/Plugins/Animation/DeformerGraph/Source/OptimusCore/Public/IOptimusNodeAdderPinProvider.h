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
* Interface that provides a mechanism to query information about parameter bindings
*/
class OPTIMUSCORE_API IOptimusNodeAdderPinProvider
{
	GENERATED_BODY()

public:
	virtual bool CanAddPinFromPin(const UOptimusNodePin* InSourcePin, EOptimusNodePinDirection InNewPinDirection, FString* OutReason = nullptr) const = 0;

	virtual UOptimusNodePin* TryAddPinFromPin(UOptimusNodePin* InSourcePin, FName InNewPinName) = 0;
	
	virtual bool RemoveAddedPin(UOptimusNodePin* InAddedPinToRemove) = 0;

	virtual FName GetSanitizedNewPinName(FName InPinName)= 0;
};
