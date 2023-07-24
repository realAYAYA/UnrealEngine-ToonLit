// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "SearchProjectSettings.generated.h"

struct FDirectoryPath;

UCLASS(config = Editor, defaultconfig, meta=(DisplayName="Search"))
class USearchProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USearchProjectSettings();

	UPROPERTY(config, EditAnywhere, Category=General)
	TArray<FDirectoryPath> IgnoredPaths;
};
