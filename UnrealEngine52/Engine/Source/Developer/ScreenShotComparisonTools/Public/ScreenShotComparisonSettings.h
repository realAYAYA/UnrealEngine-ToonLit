// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ScreenShotComparisonSettings.generated.h"

/**
* Holds settings for screenshot fallbacks
*/
USTRUCT()
struct FScreenshotFallbackEntry
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(Config)
	FString Parent;

	UPROPERTY(Config)
	FString Child;
};


UCLASS(config = Engine, defaultconfig)
class SCREENSHOTCOMPARISONTOOLS_API UScreenShotComparisonSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	* if true, any checked-in test results for confidential platforms will be put under <ProjectDir>/Platforms/<Platform>/Test instead of <ProjectDir>/Test
	*/
	UPROPERTY(Config)
	bool bUseConfidentialPlatformPathsForSavedResults;

	/**
	* An array of entries that describe other platforms we can use for fallbacks when comparing screenshots
	*/
	UPROPERTY(Config)
	TArray<FScreenshotFallbackEntry>  ScreenshotFallbackPlatforms;
};

