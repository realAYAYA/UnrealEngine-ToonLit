// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAnimNextRigVMParameterInterface.generated.h"

struct FAnimNextParamType;
struct FInstancedPropertyBag;

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class ANIMNEXTUNCOOKEDONLY_API UAnimNextRigVMParameterInterface : public UInterface
{
	GENERATED_BODY()
};

class ANIMNEXTUNCOOKEDONLY_API IAnimNextRigVMParameterInterface
{
	GENERATED_BODY()

public:
	// Get the parameter type
	virtual FAnimNextParamType GetParamType() const = 0;

	// Set the parameter type
	virtual bool SetParamType(const FAnimNextParamType& InType, bool bSetupUndoRedo = true) = 0;

	// Access the backing storage property bag for the parameter
	virtual FInstancedPropertyBag& GetPropertyBag() const = 0;
};