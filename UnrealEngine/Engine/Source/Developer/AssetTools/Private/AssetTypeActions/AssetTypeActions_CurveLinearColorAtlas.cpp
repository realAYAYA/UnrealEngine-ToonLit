// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_CurveLinearColorAtlas.h"
#include "ToolMenus.h"
#include "Engine/Texture2D.h"
#include "Styling/AppStyle.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_CurveLinearColorAtlas::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	FAssetTypeActions_Texture::GetActions(InObjects, Section);
}


#undef LOCTEXT_NAMESPACE
