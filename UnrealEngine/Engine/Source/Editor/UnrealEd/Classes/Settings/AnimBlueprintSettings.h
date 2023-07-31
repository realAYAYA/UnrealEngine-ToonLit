// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "AnimBlueprintSettings.generated.h"

/**
 * Implements Editor settings for animation blueprints
 */
UCLASS(config = EditorPerProjectUserSettings)
class UNREALED_API UAnimBlueprintSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Whether to allow event graphs to be created/displayed in animation blueprints */
	UPROPERTY()
	bool bAllowEventGraphs = true;

	/** Whether to allow macros to be created/displayed in animation blueprints */
	UPROPERTY()
	bool bAllowMacros = true;

	/** Whether to allow delegates to be created/displayed in animation blueprints */
	UPROPERTY()
	bool bAllowDelegates = true;

	/** Whether to allow restrict which base function overrides can created/displayed in animation blueprints */
	UPROPERTY()
	bool bRestrictBaseFunctionOverrides = false;

	/** The set of allowed base functions if restricted */
	UPROPERTY()
	TArray<FName> BaseFunctionOverrideAllowList;
};