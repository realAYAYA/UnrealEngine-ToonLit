// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "InterchangeImportTestSettings.generated.h"


/**
 * Implement settings for the Interchange Import Test
 */
UCLASS(config=Engine, defaultconfig)
class INTERCHANGETESTS_API UInterchangeImportTestSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category=Automation)
	FString ImportTestsPath;

	UPROPERTY(EditAnywhere, config, Category=Automation)
	TArray<FString> ImportFiles;
};
