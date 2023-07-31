// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Commandlets/WorldPartitionConvertCommandlet.h"
#include "WorldPartitionEditorSettings.generated.h"

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "World Partition"))
class UWorldPartitionEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldPartitionEditorSettings();

	UPROPERTY(Config, EditAnywhere, Category = MapConversion, Meta = (ToolTip = "Commandlet class to use for World Parition conversion"))
	TSubclassOf<UWorldPartitionConvertCommandlet> CommandletClass;

	UPROPERTY(Config, EditAnywhere, Category = Foliage, Meta = (ClampMin=3200, ToolTip= "Editor grid size used for instance foliage actors in World Partition worlds"))
	int32 InstancedFoliageGridSize;

	UPROPERTY(Config, EditAnywhere, Category = MiniMap, Meta = (ClampMin = 100, ToolTip = "Threshold from which minimap generates a warning if its WorldUnitsPerPixel is above this value"))
	int32 MinimapLowQualityWorldUnitsPerPixelThreshold;

	bool bDisableLoadingInEditor;
	bool bDisablePIE;
};