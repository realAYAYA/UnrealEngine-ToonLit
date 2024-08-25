// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionEditorSettings.h"
#include "WorldPartition/WorldPartition.h"

UWorldPartitionEditorSettings::UWorldPartitionEditorSettings()
{
	CommandletClass = UWorldPartitionConvertCommandlet::StaticClass();
	InstancedFoliageGridSize = 25600;
	MinimapLowQualityWorldUnitsPerPixelThreshold = 12800;
	bEnableLoadingInEditor = true;
	bEnableStreamingGenerationLogOnPIE = true;
	bShowHLODsInEditor = true;
	bShowHLODsOverLoadedRegions = false;
	HLODMinDrawDistance = 12800;
	HLODMaxDrawDistance = 0;
	bDisablePIE = false;
	bDisableBugIt = false;
	bAdvancedMode = true;
}

void UWorldPartitionEditorSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UWorldPartitionEditorSettings, bEnableLoadingInEditor))
	{	
		if (UWorldPartition* WorldPartition = GWorld ? GWorld->GetWorldPartition() : nullptr)
		{
			WorldPartition->OnEnableLoadingInEditorChanged();
		}
	}
}