// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ChaosClothAssetEditorModule.h"
#include "ChaosClothAsset/ClothEditorCommands.h"
#include "ChaosClothAsset/ClothEditorMode.h"
#include "ChaosClothAsset/ClothEditorStyle.h"
#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"

namespace UE::Chaos::ClothAsset
{
	void FChaosClothAssetEditorModule::StartupModule()
	{
		FChaosClothAssetEditorStyle::Get(); // Causes the constructor to be called

		FChaosClothAssetEditorCommands::Register();
	}

	void FChaosClothAssetEditorModule::ShutdownModule()
	{
		FChaosClothAssetEditorCommands::Unregister();

		FEditorModeRegistry::Get().UnregisterMode(UChaosClothAssetEditorMode::EM_ChaosClothAssetEditorModeId);
	}
} // namespace UE::Chaos::ClothAsset

IMPLEMENT_MODULE(UE::Chaos::ClothAsset::FChaosClothAssetEditorModule, ChaosClothAssetEditor)
