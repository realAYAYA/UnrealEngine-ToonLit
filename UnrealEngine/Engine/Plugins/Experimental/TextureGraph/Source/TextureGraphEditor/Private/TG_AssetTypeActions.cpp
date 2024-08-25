// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_AssetTypeActions.h"
#include "TextureGraphEditorModule.h"
#include "TextureGraph.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_TSX"

FText	FAssetTypeActions_TSX::GetName() const
{
    return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TSX", "Texture Graph");
}
FColor	FAssetTypeActions_TSX::GetTypeColor() const
{
    return FColor::Emerald;
}
UClass* FAssetTypeActions_TSX::GetSupportedClass() const
{
    return UTextureGraph::StaticClass();
}
uint32	FAssetTypeActions_TSX::GetCategories()
{
    return EAssetTypeCategories::Textures;
}


void FAssetTypeActions_TSX::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor /*= TSharedPtr<IToolkitHost>()*/)
{
    UE_LOG(LogTemp, Log, TEXT("AssetTypeActions_TSXAsset : OpenAssetEditor called "));


	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UTextureGraph* TextureGraph = Cast<UTextureGraph>(*ObjIt);

		if (TextureGraph != NULL)
		{
			FTextureGraphEditorModule* EditorModule = &FModuleManager::LoadModuleChecked<FTextureGraphEditorModule>("TextureGraphEditor");
			EditorModule->CreateTextureGraphEditor(Mode, EditWithinLevelEditor, TextureGraph);
		}
	}

}

#undef LOCTEXT_NAMESPACE