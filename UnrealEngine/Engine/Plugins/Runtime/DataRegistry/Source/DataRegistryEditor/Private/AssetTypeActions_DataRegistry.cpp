// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DataRegistry.h"

#include "DataRegistryEditorToolkit.h"
#include "DataRegistry.h"

#define LOCTEXT_NAMESPACE "DataRegistryEditor"

FText FAssetTypeActions_DataRegistry::GetName() const
{
	return LOCTEXT("AssetTypeActions_DataRegistry", "Data Registry");
}

UClass* FAssetTypeActions_DataRegistry::GetSupportedClass() const
{
	return UDataRegistry::StaticClass();
}

void FAssetTypeActions_DataRegistry::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Obj : InObjects)
	{
		if (UDataRegistry* Asset = Cast<UDataRegistry>(Obj))
		{
			FDataRegistryEditorToolkit::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Asset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
