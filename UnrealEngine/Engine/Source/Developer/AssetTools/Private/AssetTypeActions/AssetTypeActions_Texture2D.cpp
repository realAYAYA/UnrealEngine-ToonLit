// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Texture2D.h"
#include "ToolMenus.h"
#include "Misc/PackageName.h"
#include "Styling/AppStyle.h"
#include "Factories/SlateBrushAssetFactory.h"
#include "Slate/SlateBrushAsset.h"
#include "Factories/VolumeTextureFactory.h"
#include "Factories/Texture2DArrayFactory.h"
#include "Engine/VolumeTexture.h"
#include "Engine/Texture2DArray.h"
#include "AssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Texture2D::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	FAssetTypeActions_Texture::GetActions(InObjects, Section);

	auto Textures = GetTypedWeakObjectPtrs<UTexture2D>(InObjects);

	Section.AddMenuEntry(
		"Texture2D_CreateSlateBrush",
		LOCTEXT("Texture2D_CreateSlateBrush", "Create Slate Brush"),
		LOCTEXT("Texture2D_CreateSlateBrushToolTip", "Creates a new slate brush using this texture."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SlateBrushAsset"),
		FUIAction(FExecuteAction::CreateSP( this, &FAssetTypeActions_Texture2D::ExecuteCreateSlateBrush, Textures ), FCanExecuteAction())
		);

	static const auto AllowTextureArrayAssetCreationVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowTexture2DArrayCreation"));
	if (Textures.Num() > 0 && AllowTextureArrayAssetCreationVar->GetValueOnGameThread() != 0)
	{
		Section.AddMenuEntry(
			"Texture_Texture2DArray",
			LOCTEXT("Texture_Texture2DArray", "Create Texture Array"),
			LOCTEXT("Texture_CreateTexture2DArrayTooltip", "Creates a new texture array."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"),
			FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_Texture2D::ExecuteCreateTextureArray, Textures), FCanExecuteAction())
			);
	}

	if (InObjects.Num() == 1)
	{
		Section.AddMenuEntry(
			"Texture2D_CreateVolumeTexture",
			LOCTEXT("Texture2D_CreateVolumeTexture", "Create Volume Texture"),
			LOCTEXT("Texture2D_CreateVolumeTextureToolTip", "Creates a new volume texture using this texture."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.VolumeTexture"),
			FUIAction(FExecuteAction::CreateSP( this, &FAssetTypeActions_Texture2D::ExecuteCreateVolumeTexture, Textures ), FCanExecuteAction())
			);
	}

}

void FAssetTypeActions_Texture2D::ExecuteCreateSlateBrush(TArray<TWeakObjectPtr<UTexture2D>> Objects)
{
	const FString DefaultSuffix = TEXT("_Brush");

	if( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if( Object )
		{
			// Determine the asset name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USlateBrushAssetFactory* Factory = NewObject<USlateBrushAssetFactory>();
			Factory->InitialTexture = Object;
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USlateBrushAsset::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for( auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt )
		{
			auto Object = (*ObjIt).Get();
			if( Object )
			{
				// Determine the asset name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USlateBrushAssetFactory* Factory = NewObject<USlateBrushAssetFactory>();
				Factory->InitialTexture = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USlateBrushAsset::StaticClass(), Factory);

				if( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

void FAssetTypeActions_Texture2D::ExecuteCreateTextureArray(TArray<TWeakObjectPtr<UTexture2D>> Objects)
{
	const FString DefaultSuffix = TEXT("_Array");
	FString Name;
	FString PackagePath;
	CreateUniqueAssetName(Objects[0].Get()->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);
	
	// Create the factory used to generate the asset.
	UTexture2DArrayFactory* Factory = NewObject<UTexture2DArrayFactory>();
	Factory->InitialTextures.Empty();

	// Give the selected textures to the factory.
	for (int32 TextureIndex = 0; TextureIndex < Objects.Num(); ++TextureIndex)
	{
		Factory->InitialTextures.Add(Objects[TextureIndex].Get());
	}

	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UTexture2DArray::StaticClass(), Factory);
	}		
}

void FAssetTypeActions_Texture2D::ExecuteCreateVolumeTexture(TArray<TWeakObjectPtr<UTexture2D>> Objects)
{
	const FString DefaultSuffix = TEXT("_Volume");

	if( Objects.Num() == 1 )
	{
		UTexture2D* Object = Objects[0].Get();

		if( Object )
		{
			// Determine the asset name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			UVolumeTextureFactory* Factory = NewObject<UVolumeTextureFactory>();
			Factory->InitialTexture = Object;
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UVolumeTexture::StaticClass(), Factory);
		}
	}
}


#undef LOCTEXT_NAMESPACE
