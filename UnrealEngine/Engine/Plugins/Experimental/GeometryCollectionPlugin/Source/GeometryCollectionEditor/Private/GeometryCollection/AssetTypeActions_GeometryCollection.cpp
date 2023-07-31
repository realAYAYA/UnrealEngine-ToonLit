// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/AssetTypeActions_GeometryCollection.h"

#include "GeometryCollection/GeometryCollectionEditorToolkit.h"
#include "GeometryCollection/GeometryCollectionEditorPlugin.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "ToolMenus.h"

bool bGeometryCollectionDataflowEditor = false;
FAutoConsoleVariableRef CVarGeometryCollectionDataflowEditor(TEXT("p.Chaos.GeometryCollection.DataflowEditor"), bGeometryCollectionDataflowEditor, TEXT("Enable dataflow asset editor on geometry collection assets(Curently Dev-Only)"));


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
	if (bGeometryCollectionDataflowEditor)
	{
		EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
		for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			if (auto Object = Cast<UGeometryCollection>(*ObjIt))
			{
				IGeometryCollectionEditorPlugin* EditorModule = &FModuleManager::LoadModuleChecked<IGeometryCollectionEditorPlugin>("GeometryCollectionEditor");
				EditorModule->CreateGeometryCollectionAssetEditor(Mode, EditWithinLevelEditor, Object);
			}
		}
	}
	else
	{ 
		FAssetTypeActions_Base::OpenAssetEditor(InObjects,EditWithinLevelEditor);
	}
}

#undef LOCTEXT_NAMESPACE
