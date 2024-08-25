// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "AnimNextObjectAccessorConfig.generated.h"

USTRUCT()
struct FAnimNextObjectAccessorConfig
{
	GENERATED_BODY()

	/** The class to expose to AnimNext */
	UPROPERTY(EditAnywhere, Config, Category="Adapter")
	TSoftClassPtr<UObject> Class;

	/**
	 * The 'namespace' for the object when exposed to AnimNext
	 * If Accessor Name is set to 'MyObject', then all properties and functions derived from this class will have the
	 * form 'MyObject.Param'.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Adapter", meta = (CustomWidget = "ParamName"))
	FName AccessorName;
};
