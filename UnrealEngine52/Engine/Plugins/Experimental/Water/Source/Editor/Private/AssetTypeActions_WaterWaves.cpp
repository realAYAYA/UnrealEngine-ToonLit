// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_WaterWaves.h"
#include "WaterWaves.h"
#include "WaterEditorModule.h"

#define LOCTEXT_NAMESPACE "WaterWaves"

FText FAssetTypeActions_WaterWaves::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_WaterWaves", "Water Waves");
}

UClass* FAssetTypeActions_WaterWaves::GetSupportedClass() const 
{
	return UWaterWavesAsset::StaticClass();
}

uint32 FAssetTypeActions_WaterWaves::GetCategories() 
{
	return FWaterEditorModule::GetAssetCategory();
}

void FAssetTypeActions_WaterWaves::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (auto Object = Cast<UWaterWavesAsset>(*ObjIt))
		{
			FWaterEditorModule* WaterEditorModule = &FModuleManager::LoadModuleChecked<FWaterEditorModule>("WaterEditor");
			WaterEditorModule->CreateWaterWaveAssetEditor(Mode, EditWithinLevelEditor, Object);
		}
	}
}
#undef LOCTEXT_NAMESPACE