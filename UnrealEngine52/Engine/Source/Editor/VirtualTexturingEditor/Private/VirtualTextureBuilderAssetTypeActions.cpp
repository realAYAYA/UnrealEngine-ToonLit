// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureBuilderAssetTypeActions.h"

#include "VT/VirtualTextureBuilder.h"

#define LOCTEXT_NAMESPACE "VirtualTexturingEditorModule"

UClass* FAssetTypeActions_VirtualTextureBuilder::GetSupportedClass() const
{
	return UVirtualTextureBuilder::StaticClass();
}

FText FAssetTypeActions_VirtualTextureBuilder::GetName() const
{
	return LOCTEXT("AssetTypeActions_VirtualTextureBuilder", "Virtual Texture Builder"); 
}

FColor FAssetTypeActions_VirtualTextureBuilder::GetTypeColor() const 
{
	return FColor(128, 128, 128); 
}

uint32 FAssetTypeActions_VirtualTextureBuilder::GetCategories() 
{
	return EAssetTypeCategories::Textures; 
}

#undef LOCTEXT_NAMESPACE
