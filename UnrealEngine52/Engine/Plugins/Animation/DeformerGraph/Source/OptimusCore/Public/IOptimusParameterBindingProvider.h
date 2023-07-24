// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "OptimusBindingTypes.h"

#include "IOptimusParameterBindingProvider.generated.h"

UINTERFACE()
class OPTIMUSCORE_API UOptimusParameterBindingProvider :
	public UInterface
{
	GENERATED_BODY()
};

/**
* Interface that provides a mechanism to query information about parameter bindings
*/
class OPTIMUSCORE_API IOptimusParameterBindingProvider
{
	GENERATED_BODY()

public:
	virtual FString GetBindingDeclaration(FName BindingName) const = 0;
};
