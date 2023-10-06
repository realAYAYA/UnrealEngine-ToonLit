// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerAssetDefinitions.h"
#include "MLDeformerAsset.h"
#include "MLDeformerEditorToolkit.h"

#define LOCTEXT_NAMESPACE "MLDeformer_AssetTypeActions"

FText UAssetDefinition_MLDeformer::GetAssetDisplayName() const
{ 
	return LOCTEXT("AssetTypeActions_MLPoser", "ML Deformer");
}

FLinearColor UAssetDefinition_MLDeformer::GetAssetColor() const
{
	return FColor(255, 255, 0);
}

TSoftClassPtr<UObject> UAssetDefinition_MLDeformer::GetAssetClass() const
{ 
	return UMLDeformerAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MLDeformer::GetAssetCategories() const
{ 
	static const auto Categories = { EAssetCategoryPaths::Animation }; 
	return Categories;
}

EAssetCommandResult UAssetDefinition_MLDeformer::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::MLDeformer;
	for (UMLDeformerAsset* Asset : OpenArgs.LoadObjects<UMLDeformerAsset>())
	{
		TSharedRef<FMLDeformerEditorToolkit> NewEditor(new FMLDeformerEditorToolkit());
		NewEditor->InitAssetEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
