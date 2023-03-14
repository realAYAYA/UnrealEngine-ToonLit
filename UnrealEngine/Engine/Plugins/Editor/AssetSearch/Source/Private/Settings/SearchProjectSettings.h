// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "SearchProjectSettings.generated.h"

UCLASS(config = Editor, defaultconfig, meta=(DisplayName="Search"))
class USearchProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USearchProjectSettings();

	UPROPERTY(config, EditAnywhere, Category=General)
	TArray<FDirectoryPath> IgnoredPaths;
};
