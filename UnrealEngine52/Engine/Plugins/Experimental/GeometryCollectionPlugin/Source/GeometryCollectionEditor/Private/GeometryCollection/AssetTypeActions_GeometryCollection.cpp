// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/AssetTypeActions_GeometryCollection.h"

#include "Dataflow/DataflowEditorPlugin.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_GeometryCollection::GetSupportedClass() const
{
	return UGeometryCollection::StaticClass();
}

UThumbnailInfo* FAssetTypeActions_GeometryCollection::GetThumbnailInfo(UObject* Asset) const
{
	UGeometryCollection * GeometryCollection = CastChecked<UGeometryCollection>(Asset);
	UThumbnailInfo* ThumbnailInfo = GeometryCollection->ThumbnailInfo;
	if (ThumbnailInfo == nullptr)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(GeometryCollection, NAME_None, RF_Transactional);
		GeometryCollection->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_GeometryCollection::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	bool bNeedsBaseEditor = true;

	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (auto Object = Cast<UGeometryCollection>(*ObjIt))
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
