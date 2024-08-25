// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/AssetDefinition_GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEditorModule.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Math/Color.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Toolkits/SimpleAssetEditor.h"


#define LOCTEXT_NAMESPACE "AssetActions_GeometryCollection"


namespace UE::GeometryCollection
{
	struct FColorScheme
	{
		static inline const FLinearColor Asset = FColor(180, 120, 110);
		static inline const FLinearColor NodeHeader = FColor(180, 120, 110);
		static inline const FLinearColor NodeBody = FColor(18, 12, 11, 127);
	};
}

FText UAssetDefinition_GeometryCollection::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GeometryCollection", "Geometry Collection");
}

TSoftClassPtr<UObject> UAssetDefinition_GeometryCollection::GetAssetClass() const
{
	return UGeometryCollection::StaticClass();
}

FLinearColor UAssetDefinition_GeometryCollection::GetAssetColor() const
{
	return UE::GeometryCollection::FColorScheme::Asset;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_GeometryCollection::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Physics };
	return Categories;
}

UThumbnailInfo* UAssetDefinition_GeometryCollection::LoadThumbnailInfo(const FAssetData& InAsset) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAsset.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_GeometryCollection::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UGeometryCollection*> GeometryCollectionObjects = OpenArgs.LoadObjects<UGeometryCollection>();

	// For now the geometry collection editor only works on one asset at a time
	ensure(GeometryCollectionObjects.Num() == 0 || GeometryCollectionObjects.Num() == 1);
	if (GeometryCollectionObjects.Num() == 1)
	{
		// Validate the asset
		if (UGeometryCollection* const GeometryCollection = Cast<UGeometryCollection>(GeometryCollectionObjects[0]))
		{
			if (FDataflowEditorToolkit::HasDataflowAsset(GeometryCollection))
			{
				UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
				UDataflowEditor* const AssetEditor = NewObject<UDataflowEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
				AssetEditor->Initialize({ GeometryCollection });
				return EAssetCommandResult::Handled;
			}

			FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, GeometryCollection);
			return EAssetCommandResult::Handled;

		}
	}
	return EAssetCommandResult::Unhandled;
}


#undef LOCTEXT_NAMESPACE
