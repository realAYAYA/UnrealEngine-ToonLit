// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VirtualScoutingBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class VIRTUALSCOUTINGEDITOR_API UVirtualScoutingBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category="VirtualScouting")
	static bool CheckIsWithEditor();

};
