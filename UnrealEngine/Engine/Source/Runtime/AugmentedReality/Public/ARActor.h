// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARTypes.h"
#include "ARComponent.h"
#include "Engine/GameEngine.h"
#include "ARActor.generated.h"

class UARComponent;

UCLASS(BlueprintType, Experimental, Category="AR Gameplay")
class AUGMENTEDREALITY_API AARActor: public AActor
{
	GENERATED_BODY()
	
public:
	AARActor();
	
	UFUNCTION(BlueprintCallable, Category="AR Gameplay")
	UARComponent* AddARComponent(TSubclassOf<UARComponent> InComponentClass, const FGuid& NativeID);
	
	static void RequestSpawnARActor(FGuid NativeID, UClass* InComponentClass);
	static void RequestDestroyARActor(AARActor* InActor);
};

USTRUCT()
struct AUGMENTEDREALITY_API FTrackedGeometryGroup
{
public:
	GENERATED_USTRUCT_BODY()
	
	FTrackedGeometryGroup() = default;
	
	FTrackedGeometryGroup(UARTrackedGeometry* InTrackedGeometry);
	
	UPROPERTY()
	TObjectPtr<AARActor> ARActor = nullptr;
	
	UPROPERTY()
	TObjectPtr<UARComponent> ARComponent = nullptr;
	
	UPROPERTY()
	TObjectPtr<UARTrackedGeometry> TrackedGeometry = nullptr;
};
