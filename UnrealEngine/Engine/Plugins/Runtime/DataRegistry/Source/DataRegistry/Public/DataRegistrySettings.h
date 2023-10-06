// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "DataRegistrySettings.generated.h"

struct FDirectoryPath;

struct FPropertyChangedEvent;


/** Settings for the Data Registry subsystem, these settings are used to scan for registry assets and set runtime access rules */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Data Registry"))
class DATAREGISTRY_API UDataRegistrySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	
	/** List of directories to scan for data registry assets */
	UPROPERTY(config, EditAnywhere, Category = "Data Registry", meta = (RelativeToGameContentDir, LongPackageName))
	TArray<FDirectoryPath> DirectoriesToScan;

	/** If false, only registry assets inside DirectoriesToScan will be initialized. If true, it will also initialize any in-memory DataRegistry assets outside the scan paths */
	UPROPERTY(config, EditAnywhere, Category = "Data Registry")
	bool bInitializeAllLoadedRegistries = false;

	/** If true, cooked builds will ignore errors with missing AssetRegistry data for specific registered assets like DataTables as it may have been stripped out */
	UPROPERTY(config, EditAnywhere, Category = "Data Registry")
	bool bIgnoreMissingCookedAssetRegistryData = false;


	/** Return true if we are allowed to ignore missing asset registry data based on settings and build */
	bool CanIgnoreMissingAssetData() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "DataRegistryTypes.h"
#endif
