// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
#include "Engine/EngineTypes.h"
#endif
#include "Engine/ActorInstanceHandle.h"

#include "LightWeightInstanceBlueprintFunctionLibrary.generated.h"

UCLASS()
class ENGINE_API ULightWeightInstanceBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Returns a handle to a new light weight instance that represents an object of type ActorClass
	UFUNCTION(BlueprintCallable, Category = "Light Weight Instance")
	static FActorInstanceHandle CreateNewLightWeightInstance(UClass* ActorClass, FTransform Transform, UDataLayerInstance* Layer, UWorld* World);

	// Returns a handle to the light weight representation and destroys Actor if successful; Returns a handle to Actor otherwise
	UFUNCTION(BlueprintCallable, Category = "Light Weight Instance")
	static FActorInstanceHandle ConvertActorToLightWeightInstance(AActor* Actor);
};