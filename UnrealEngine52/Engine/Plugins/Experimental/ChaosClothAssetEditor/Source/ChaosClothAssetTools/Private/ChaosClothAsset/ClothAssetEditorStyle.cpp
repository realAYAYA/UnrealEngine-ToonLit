// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothAssetEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

namespace UE::Chaos::ClothAsset
{
	TOptional<FClothAssetEditorStyle> FClothAssetEditorStyle::Singleton;

	FClothAssetEditorStyle::FClothAssetEditorStyle()
		: FSlateStyleSet("ClothAssetEditorStyle")
	{
		SetContentRoot(IPluginManager::Get().FindPlugin("ChaosClothAssetEditor")->GetBaseDir() / TEXT("Resources"));

		Set("ClassIcon.ChaosClothAsset", new FSlateVectorImageBrush(RootToContentDir(TEXT("ClothAsset_16.svg")), FVector2D(16)));
		Set("ClassThumbnail.ChaosClothAsset", new FSlateVectorImageBrush(RootToContentDir(TEXT("ClothAsset_64.svg")), FVector2D(64)));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	FClothAssetEditorStyle::~FClothAssetEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
}  // End namespace UE::Chaos::ClothAsset
