// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetsTools/AssetTypeActions_DMXPixelMapping.h"
#include "DMXPixelMapping.h"
#include "DMXPixelMappingEditorCommon.h"
#include "DMXPixelMappingEditorModule.h"
#include "Toolkits/DMXPixelMappingToolkit.h"

#include "Toolkits/IToolkitHost.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_DMXPixelMapping"

FText FAssetTypeActions_DMXPixelMapping::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DMXPixelMapping", "DMX Pixel Mapping");
}

UClass* FAssetTypeActions_DMXPixelMapping::GetSupportedClass() const
{
	return UDMXPixelMapping::StaticClass();
}

void FAssetTypeActions_DMXPixelMapping::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (UDMXPixelMapping* DMXPixelMapping = Cast<UDMXPixelMapping>(Object))
		{
			TSharedRef<FDMXPixelMappingToolkit> PixelMappingToolkit(MakeShared<FDMXPixelMappingToolkit>());
			PixelMappingToolkit->InitPixelMappingEditor(Mode, EditWithinLevelEditor, DMXPixelMapping);
		}
		else
		{
			UE_LOG(LogDMXPixelMappingEditor, Warning, TEXT("Wrong object class for pixel mapping editor %"), *Object->GetClass()->GetFName().ToString());
		}
	}
}

uint32 FAssetTypeActions_DMXPixelMapping::GetCategories()
{
	return uint32(FDMXPixelMappingEditorModule::GetAssetCategory());
}

#undef LOCTEXT_NAMESPACE