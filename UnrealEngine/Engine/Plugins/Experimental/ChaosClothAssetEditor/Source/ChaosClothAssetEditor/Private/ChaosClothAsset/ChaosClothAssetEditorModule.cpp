// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ChaosClothAssetEditorModule.h"
#include "ChaosClothAsset/ClothEditorMode.h"
#include "ChaosClothAsset/ClothEditorCommands.h"
#include "ChaosClothAsset/ClothEditorStyle.h"
#include "ContentBrowserMenuContexts.h"
#include "EditorModeRegistry.h"
#include "Selection.h"
#include "ToolMenus.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "FChaosClothAssetEditorModule"

namespace UE::Chaos::ClothAsset
{
void FChaosClothAssetEditorModule::StartupModule()
{
	FChaosClothAssetEditorStyle::Get(); // Causes the constructor to be called

	FChaosClothAssetEditorCommands::Register();

	// Register asset actions
	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// TODO: Register details view customizations
}

void FChaosClothAssetEditorModule::ShutdownModule()
{
	FChaosClothAssetEditorCommands::Unregister();

	FEditorModeRegistry::Get().UnregisterMode(UChaosClothAssetEditorMode::EM_ChaosClothAssetEditorModeId);

	// TODO: Unregister details view customizations
}
} // namespace UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(UE::Chaos::ClothAsset::FChaosClothAssetEditorModule, ChaosClothAssetEditor)