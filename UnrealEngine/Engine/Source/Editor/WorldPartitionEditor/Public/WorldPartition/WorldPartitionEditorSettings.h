// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Commandlets/WorldPartitionConvertCommandlet.h"
#include "WorldPartitionEditorSettings.generated.h"

UCLASS(config = EditorSettings, meta = (DisplayName = "World Partition"))
class WORLDPARTITIONEDITOR_API UWorldPartitionEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldPartitionEditorSettings();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	UPROPERTY(Config, EditAnywhere, Category = MapConversion, Meta = (ToolTip = "Commandlet class to use for World Partition conversion"))
	TSubclassOf<UWorldPartitionConvertCommandlet> CommandletClass;

	UPROPERTY(Config, EditAnywhere, Category = Foliage, Meta = (ClampMin = 3200, ToolTip= "Editor grid size used for instance foliage actors in World Partition worlds"))
	int32 InstancedFoliageGridSize;

	UPROPERTY(Config, EditAnywhere, Category = MiniMap, Meta = (ClampMin = 100, ToolTip = "Threshold from which minimap generates a warning if its WorldUnitsPerPixel is above this value"))
	int32 MinimapLowQualityWorldUnitsPerPixelThreshold;

	UPROPERTY(Config, EditAnywhere, Category = WorldPartition, Meta = (ToolTip = "Whether to enable dynamic loading in the editor through loading regions"))
	bool bEnableLoadingInEditor;

	UPROPERTY(Config, EditAnywhere, Category = WorldPartition, Meta = (ToolTip = "Whether to enable streaming generation log on PIE"))
	bool bEnableStreamingGenerationLogOnPIE;

	UPROPERTY(Config, EditAnywhere, Category = HLOD, Meta = (ToolTip = "Whether to show HLODs in the editor"))
	bool bShowHLODsInEditor;

	UPROPERTY(Config, EditAnywhere, Category = HLOD, Meta = (ToolTip = "Control display of HLODs in case actors are loaded"))
	bool bShowHLODsOverLoadedRegions;

	UPROPERTY(Config, EditAnywhere, Category = HLOD, Meta = (UIMin = 0, UIMax = 1638400, DisplayName = "HLOD Min Draw Distance", ToolTip = "Minimum distance at which HLODs should be displayed in editor"))
	double HLODMinDrawDistance;

	UPROPERTY(Config, EditAnywhere, Category = HLOD, Meta = (UIMin = 0, UIMax = 1638400, DisplayName = "HLOD Max Draw Distance", ToolTip = "Maximum distance at which HLODs should be displayed in editor"))
	double HLODMaxDrawDistance;

	bool bDisableBugIt;
	bool bDisablePIE;
	bool bAdvancedMode;
};