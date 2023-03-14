// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_TextureCube.h"
#include "ToolMenus.h"
#include "Misc/PackageName.h"
#include "Styling/AppStyle.h"
#include "Factories/SlateBrushAssetFactory.h"
#include "Slate/SlateBrushAsset.h"
#include "Factories/TextureCubeArrayFactory.h"
#include "Engine/TextureCubeArray.h"
#include "AssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_TextureCube::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	FAssetTypeActions_Texture::GetActions(InObjects, Section);

	auto Textures = GetTypedWeakObjectPtrs<UTextureCube>(InObjects);

	if (Textures.Num() > 0)
	{
		Section.AddMenuEntry(
			"Texture_TextureCuberray",
			LOCTEXT("Texture_TextureCuberray", "Create Texture Cube Array"),
			LOCTEXT("Texture_CreateTextureCubeArrayTooltip", "Creates a new texture cube array."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.TextureCube"),
			FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_TextureCube::ExecuteCreateTextureArray, Textures), FCanExecuteAction())
		);
	}
}

void FAssetTypeActions_TextureCube::ExecuteCreateTextureArray(TArray<TWeakObjectPtr<UTextureCube>> Objects)
{
	const FString DefaultSuffix = TEXT("_Array");
	FString Name;
	FString PackagePath;
	CreateUniqueAssetName(Objects[0].Get()->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

	// Create the factory used to generate the asset.
	UTextureCubeArrayFactory* Factory = NewObject<UTextureCubeArrayFactory>();
	Factory->InitialTextures.Empty();

	// Give the selected textures to the factory.
	for (int32 TextureIndex = 0; TextureIndex < Objects.Num(); ++TextureIndex)
	{
		Factory->InitialTextures.Add(Objects[TextureIndex].Get());
	}

	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UTextureCubeArray::StaticClass(), Factory);
	}
}

#undef LOCTEXT_NAMESPACE
