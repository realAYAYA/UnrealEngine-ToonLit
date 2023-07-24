// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IAnimNextInterfaceParameters.generated.h"

namespace UE::AnimNext::Interface
{
struct FParam;
struct FContext;
}

UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class ANIMNEXTINTERFACE_API UAnimNextInterfaceParameters : public UInterface
{
	GENERATED_BODY()
};

// Interface representing parameters that can be supplied to an anim interface
class ANIMNEXTINTERFACE_API IAnimNextInterfaceParameters
{
	GENERATED_BODY()

	friend struct UE::AnimNext::Interface::FContext;
	
	// Get a parameter by name
	virtual bool GetParameter(FName InKey, UE::AnimNext::Interface::FParam& OutParam) = 0;

	// Get a parameter pointer by name
	virtual const UE::AnimNext::Interface::FParam* GetParameter(FName InKey) = 0;

	// Default utility implementation for UObjects
	// Treats key as a property name or function
	static bool GetParameterForObject(UObject* InObject, FName InKey, UE::AnimNext::Interface::FParam& OutParam);
/*	{
		checkSlow(InObject);
		const UClass* ObjectClass = InObject->GetClass();
		if(const FProperty* Property = FindFProperty<FProperty>(ObjectClass, InKey))
		{
			OutParam = UE::AnimNext::Interface::FPropertyParam(Property, InObject);
			return true;
		}
		else if(const UFunction* Function = FindUField<UFunction>(ObjectClass, InKey))
		{
			OutParam = UE::AnimNext::Interface::FFunctionParam(Property, InObject);
		}
		return false;
	}*/
};
