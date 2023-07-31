// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IDataInterfaceParameters.generated.h"

namespace UE::DataInterface
{
struct FParam;
struct FContext;
}

UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class DATAINTERFACE_API UDataInterfaceParameters : public UInterface
{
	GENERATED_BODY()
};

// Interface representing parameters that can be supplied to a data interface
class DATAINTERFACE_API IDataInterfaceParameters
{
	GENERATED_BODY()

	friend struct UE::DataInterface::FContext;
	
	// Get a parameter by name
	virtual bool GetParameter(FName InKey, UE::DataInterface::FParam& OutParam) = 0;

	// Default utility implementation for UObjects
	// Treats key as a property name or function
	static bool GetParameterForObject(UObject* InObject, FName InKey, UE::DataInterface::FParam& OutParam);
/*	{
		checkSlow(InObject);
		const UClass* ObjectClass = InObject->GetClass();
		if(const FProperty* Property = FindFProperty<FProperty>(ObjectClass, InKey))
		{
			OutParam = UE::DataInterface::FPropertyParam(Property, InObject);
			return true;
		}
		else if(const UFunction* Function = FindUField<UFunction>(ObjectClass, InKey))
		{
			OutParam = UE::DataInterface::FFunctionParam(Property, InObject);
		}
		return false;
	}*/
};
