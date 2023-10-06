// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "ChaosBlueprintLibrary.generated.h"

class UChaosEventRelay;

UCLASS()
class UChaosBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Physics Object")
	static const UChaosEventRelay* GetEventRelayFromContext(UObject* ContextObject);
};