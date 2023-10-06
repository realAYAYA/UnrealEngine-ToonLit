// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DynamicBlueprintBinding.generated.h"

UCLASS(abstract, MinimalAPI)
class UDynamicBlueprintBinding
	: public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void BindDynamicDelegates(UObject* InInstance) const { }
	virtual void UnbindDynamicDelegates(UObject* Instance) const { }
	virtual void UnbindDynamicDelegatesForProperty(UObject* InInstance, const FObjectProperty* InObjectProperty) const { }
};
