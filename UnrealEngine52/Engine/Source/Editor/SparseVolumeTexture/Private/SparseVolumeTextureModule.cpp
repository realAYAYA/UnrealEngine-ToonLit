// Copyright Epic Games, Inc. All Rights Reserved.

#include "SparseVolumeTextureModule.h"

#include "SparseVolumeTextureOpenVDB.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "SparseVolumeTextureOpenVDBUtility.h"

#define LOCTEXT_NAMESPACE "SparseVolumeTextureModule"

IMPLEMENT_MODULE(FSparseVolumeTextureModule, SparseVolumeTexture);

void FSparseVolumeTextureModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	AssetTypeActionsForSparseVolumeTexture = MakeShareable(new FAssetTypeActions_SparseVolumeTexture);
	AssetTools.RegisterAssetTypeActions(AssetTypeActionsForSparseVolumeTexture.ToSharedRef());

#if PLATFORM_WINDOWS
	// Global registration of  the vdb types.
	openvdb::initialize();
#endif

	// USparseVolumeTexture needs to access (only) this function for cooking.
	OnConvertOpenVDBToSparseVolumeTexture().BindStatic(ConvertOpenVDBToSparseVolumeTexture);
}

void FSparseVolumeTextureModule::ShutdownModule()
{
	OnConvertOpenVDBToSparseVolumeTexture().Unbind();
}

#undef LOCTEXT_NAMESPACE
