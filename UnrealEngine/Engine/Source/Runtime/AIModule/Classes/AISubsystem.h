// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Tickable.h"
#include "AISystem.h"
#include "AISubsystem.generated.h"


class UAISystem;

/** A class representing a common interface and behavior for AI subsystems */
UCLASS(config = Engine, defaultconfig)
class AIMODULE_API UAISubsystem : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

private:
	UPROPERTY()
	TObjectPtr<UAISystem> AISystem;
			
public:
	UAISubsystem(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UWorld* GetWorld() const override;

	// FTickableGameObject begin
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorldFast(); }
	virtual void Tick(float DeltaTime) override {}
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	// FTickableGameObject end

	UWorld* GetWorldFast() const { return AISystem ? AISystem->GetOuterWorld() : GetOuter()->GetWorld(); }
};
