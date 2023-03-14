// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "GameplayBehaviorSubsystem.generated.h"


class UGameplayBehavior;
class AActor;
class UGameplayBehaviorConfig;
class UWorld;

USTRUCT()
struct FAgentGameplayBehaviors
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UGameplayBehavior>> Behaviors;
};

UCLASS(config = Game, defaultconfig, Transient)
class GAMEPLAYBEHAVIORSMODULE_API UGameplayBehaviorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UGameplayBehaviorSubsystem* GetCurrent(const UWorld* World);
	static bool TriggerBehavior(const UGameplayBehaviorConfig& Config, AActor& Avatar, AActor* SmartObjectOwner = nullptr);
	static bool TriggerBehavior(UGameplayBehavior& Behavior, AActor& Avatar, const UGameplayBehaviorConfig* Config, AActor* SmartObjectOwner = nullptr);
	bool StopBehavior(AActor& Avatar, TSubclassOf<UGameplayBehavior> BehaviorToStop);

protected:
	void OnBehaviorFinished(UGameplayBehavior& Behavior, AActor& Avatar, const bool bInterrupted);

	virtual bool TriggerBehaviorImpl(UGameplayBehavior& Behavior, AActor& Avatar, const UGameplayBehaviorConfig* Config, AActor* SmartObjectOwner = nullptr);

	UPROPERTY()
	TMap<TObjectPtr<AActor>, FAgentGameplayBehaviors> AgentGameplayBehaviors;
};
