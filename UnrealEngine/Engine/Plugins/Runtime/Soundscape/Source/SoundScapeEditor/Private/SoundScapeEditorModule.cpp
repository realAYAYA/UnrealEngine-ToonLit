// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundScapeEditorModule.h"
#include "AssetToolsModule.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeActions_SoundscapeColor.h"
#include "AssetTypeActions_SoundScapePalette.h"



IMPLEMENT_MODULE(FSoundscapeEditorModule, SoundscapeEditor)

void FSoundscapeEditorModule::StartupModule()
{

	// Register the audio editor asset type actions.
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundscapeColor));
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundscapePalette));
}

void FSoundscapeEditorModule::ShutdownModule()
{
}

