// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Engine/GameInstance.h"
#include "MGameInstance.generated.h"

class UGameTables;
class UMGameSession;

/**
 * 
 */
UCLASS()
class DEMO_API UMGameInstance : public UGameInstance
{
	GENERATED_BODY()


public:

	virtual void Init() override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ProjectM")
	UMGameSession* GetMGameSession();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ProjectM")
	UGameTables* GetGameTables();
};

