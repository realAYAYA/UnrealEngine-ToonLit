// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosFlesh/Asset/AssetTypeActions_FleshAsset.h"

#include "ChaosFlesh/FleshAsset.h"
#include "Dataflow/DataflowEditorPlugin.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_FleshAsset::GetSupportedClass() const
{
	return UFleshAsset::StaticClass();
}

UThumbnailInfo* FAssetTypeActions_FleshAsset::GetThumbnailInfo(UObject* Asset) const
{
	UFleshAsset * FleshAsset = CastChecked<UFleshAsset>(Asset);
	UThumbnailInfo* ThumbnailInfo = FleshAsset->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(FleshAsset, NAME_None, RF_Transactional);
		FleshAsset->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_FleshAsset::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	FAssetTypeActions_Base::GetActions(InObjects, Section);
}

void FAssetTypeActions_FleshAsset::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	bool bNeedsBaseEditor = true;
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (auto Object = Cast<UFleshAsset>(*ObjIt))
		{
			if (FDataflowEditorToolkit::CanOpenDataflowEditor(Object))
			{
				bNeedsBaseEditor = false;
				IDataflowEditorPlugin* DataflowEditorPlugin = &FModuleManager::LoadModuleChecked<IDataflowEditorPlugin>("DataflowEditor");
				DataflowEditorPlugin->CreateDataflowAssetEditor(Mode, EditWithinLevelEditor, Object);
			}
		}
	}
	if (bNeedsBaseEditor)
	{
		FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
	}
}

#undef LOCTEXT_NAMESPACE
