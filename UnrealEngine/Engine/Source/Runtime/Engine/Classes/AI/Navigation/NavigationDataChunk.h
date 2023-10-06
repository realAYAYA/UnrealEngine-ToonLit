// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NavigationDataChunk.generated.h"

/** 
 * 
 */
UCLASS(NotBlueprintable, abstract, MinimalAPI)
class UNavigationDataChunk : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Name of NavigationData actor that owns this chunk */
	UPROPERTY()
	FName NavigationDataName;
};
