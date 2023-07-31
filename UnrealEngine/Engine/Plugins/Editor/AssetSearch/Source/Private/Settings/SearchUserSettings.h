// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "SearchUserSettings.generated.h"

USTRUCT()
struct FSearchPerformance
{
	GENERATED_BODY();

public:
	UPROPERTY(EditAnywhere, Category = "Performance")
	int32 ParallelDownloads = 0;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance")
	int32 DownloadProcessRate = 0;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Performance")
	int32 AssetScanRate = 0;
};

UCLASS(config = EditorPerProjectUserSettings, meta=(DisplayName="Search"))
class USearchUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USearchUserSettings();

	/** Enable this to begin using search. */
	UPROPERTY(config, EditAnywhere, Category = General)
	bool bEnableSearch;

	UPROPERTY(config, EditAnywhere, Category=General)
	TArray<FDirectoryPath> IgnoredPaths;

	UPROPERTY(config, EditAnywhere, Category=General)
	bool bShowMissingAssets = true;

	UPROPERTY(config, EditAnywhere, Category = General)
	bool bAutoExpandAssets = true;

	UPROPERTY(config, EditAnywhere, Category = Performance)
	bool bThrottleInBackground = true;

	UPROPERTY(config, EditAnywhere, Category = Performance)
	FSearchPerformance DefaultOptions;

	UPROPERTY(config, EditAnywhere, Category = Performance)
	FSearchPerformance BackgroundtOptions;

	const FSearchPerformance& GetPerformanceOptions() const;

public:
	UPROPERTY(Transient)
	int32 SearchInForeground = 0;
};
