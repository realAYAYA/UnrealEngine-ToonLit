// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Chaos/SimCallbackInput.h"
#include "Chaos/SimCallbackObject.h"
#include "WaterBodyTypes.h"
#include "BuoyancyTypes.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "BuoyancyManager.generated.h"

class UBuoyancyComponent;
class AWaterBody;

class FBuoyancyManagerAsyncCallback : public Chaos::TSimCallbackObject<FBuoyancyManagerAsyncInput, FBuoyancyManagerAsyncOutput>
{

public:
	void CreateAsyncAux_External(Chaos::FUniqueIdx UniqueIndex, TUniquePtr<FBuoyancyComponentAsyncAux>&& Aux);
	void ClearAsyncAux_External(Chaos::FUniqueIdx UniqueIndex);

private:
	virtual void OnPreSimulate_Internal() override;

	TMap<Chaos::FUniqueIdx, TUniquePtr<FBuoyancyComponentAsyncAux>> BuoyancyComponentToAux_Internal;
};

UCLASS()
class WATER_API ABuoyancyManager : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	static ABuoyancyManager* Get(const UObject* WorldContextObject);

	UFUNCTION()
	static bool GetBuoyancyComponentManager(const UObject* WorldContextObject, ABuoyancyManager*& Manager);

	void OnCreatePhysics(UActorComponent* Component);
	void OnDestroyPhysics(UActorComponent* Component);

	void ClearAsyncInputs(UBuoyancyComponent* Component);

	void Register(UBuoyancyComponent* BuoyancyComponent);
	void Unregister(UBuoyancyComponent* BuoyancyComponent);

	void Update(FPhysScene* PhysScene, float DeltaTime);

private:
	void InitializeAsyncAux(UBuoyancyComponent* Component);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	TSet<const UPrimitiveComponent*> PhysicsInitializedSimulatingComponents;

	UPROPERTY()
	TArray<TObjectPtr<UBuoyancyComponent>> BuoyancyComponents;
	TArray<UBuoyancyComponent*> BuoyancyComponentsActive;
	
	// List of buoyancy components to defer registration until this buoyancy manager is fully initialized.
	TArray<TWeakObjectPtr<UBuoyancyComponent>> BuoyancyComponentsToRegister;

	FBuoyancyManagerAsyncCallback* AsyncCallback;
	int32 Timestamp;

	TArray<Chaos::TSimCallbackOutputHandle<FBuoyancyManagerAsyncOutput>> PendingOutputs;
	Chaos::TSimCallbackOutputHandle<FBuoyancyManagerAsyncOutput> LatestOutput;

	FDelegateHandle OnCreateDelegateHandle;
	FDelegateHandle OnDestroyDelegateHandle;
	FDelegateHandle OnPhysScenePreTickHandle;
};