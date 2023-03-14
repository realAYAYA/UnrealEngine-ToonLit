// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "AssetTypeActions_StreamerInput.h"

class FPixelStreamingBlueprintEditorModule : public IModuleInterface
{
public:
private:
	/** IModuleInterface implementation */
	void StartupModule() override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_StreamerInput>());
	}
	void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FPixelStreamingBlueprintEditorModule, PixelStreamingBlueprintEditor)
