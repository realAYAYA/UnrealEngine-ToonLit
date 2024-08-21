// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MGameInstance.generated.h"

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

	UFUNCTION(BlueprintCallable, Category = "ProjectM")
	UMGameSession* GetMGameSession();
};

