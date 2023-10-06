// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Physics/Experimental/ChaosEventType.h"
#include "PhysicsPublic.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"

#include "ChaosEventRelay.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCollisionEventSignature, const TArray<FCollisionChaosEvent>&, CollisionEvents);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBreakEventSignature, const TArray<FChaosBreakEvent>&, BreakEvents);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRemovalEventSignature, const TArray<FChaosRemovalEvent>&, RemovalEvents);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCrumblingEventSignature, const TArray<FChaosCrumblingEvent>&, CrumblingEvents);


/**
* An object managing events
*/
UCLASS(MinimalAPI)
class UChaosEventRelay : public UObject
{
	GENERATED_BODY()

public:

	ENGINE_API UChaosEventRelay();

	ENGINE_API void DispatchPhysicsCollisionEvents(const TArray<FCollisionChaosEvent>& CollisionEvents);
	ENGINE_API void DispatchPhysicsBreakEvents(const TArray<FChaosBreakEvent>& BreakEvents);
	ENGINE_API void DispatchPhysicsRemovalEvents(const TArray<FChaosRemovalEvent>& RemovalEvents);
	ENGINE_API void DispatchPhysicsCrumblingEvents(const TArray<FChaosCrumblingEvent>& CrumblingEvents);

	
	UPROPERTY(BlueprintAssignable, Category = "Physics Events")
	FCollisionEventSignature OnCollisionEvent;

	UPROPERTY(BlueprintAssignable, Category = "Physics Events")
	FBreakEventSignature OnBreakEvent;

	UPROPERTY(BlueprintAssignable, Category = "Physics Events")
	FRemovalEventSignature OnRemovalEvent;

	UPROPERTY(BlueprintAssignable, Category = "Physics Events")
	FCrumblingEventSignature OnCrumblingEvent;

};
