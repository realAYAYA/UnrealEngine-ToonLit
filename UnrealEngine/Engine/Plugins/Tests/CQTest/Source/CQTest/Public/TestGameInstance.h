// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"

#include "TestGameInstance.generated.h"

UCLASS(config=Game, transient, BlueprintType, Blueprintable)
class CQTEST_API UTestGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()

public:
	void InitializeForTest(UWorld& InWorld);
};