// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusUnnamedNodePinProvider.generated.h"

class UOptimusNodePin;

UINTERFACE()
class OPTIMUSCORE_API UOptimusUnnamedNodePinProvider :
	public UInterface
{
	GENERATED_BODY()
};

/**
* Interface that provides a mechanism to add pins to node from existing pins
*/
class OPTIMUSCORE_API IOptimusUnnamedNodePinProvider
{
	GENERATED_BODY()

public:
	virtual bool IsPinNameHidden(UOptimusNodePin* InPin) const = 0;
	virtual FName GetNameForAdderPin(UOptimusNodePin* InPin) const = 0;
};
