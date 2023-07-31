// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassComponentHitTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/HitResult.h"

#include "MassComponentHitSubsystem.generated.h"

class UMassAgentSubsystem;
class UMassSignalSubsystem;
class UCapsuleComponent;


/**
 * Subsystem that keeps track of the latest component hits and allow mass entities to retrieve and handle them
 */
UCLASS()
class MASSAIBEHAVIOR_API UMassComponentHitSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	const FMassHitResult* GetLastHit(const FMassEntityHandle Entity) const;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	void RegisterForComponentHit(const FMassEntityHandle Entity, UCapsuleComponent& CapsuleComponent);
	void UnregisterForComponentHit(FMassEntityHandle Entity, UCapsuleComponent& CapsuleComponent);

	UFUNCTION()
	void OnHitCallback(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY()
	TObjectPtr<UMassSignalSubsystem> SignalSubsystem;

	UPROPERTY()
    TObjectPtr<UMassAgentSubsystem> AgentSubsystem;

	UPROPERTY()
	TMap<FMassEntityHandle, FMassHitResult> HitResults;

	UPROPERTY()
	TMap<TObjectPtr<UActorComponent>, FMassEntityHandle> ComponentToEntityMap;

	UPROPERTY()
	TMap<FMassEntityHandle, TObjectPtr<UActorComponent>> EntityToComponentMap;
};
