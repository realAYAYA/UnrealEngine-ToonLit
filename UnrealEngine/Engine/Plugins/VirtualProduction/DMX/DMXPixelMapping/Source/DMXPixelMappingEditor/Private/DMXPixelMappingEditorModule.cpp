// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingEditorModule.h"
#include "DMXPixelMappingEditorStyle.h"
#include "DMXPixelMappingEditorCommands.h"
#include "DMXPixelMapping.h"
#include "DMXPixelMappingThumbnailRendering.h"
#include "AssetsTools/AssetTypeActions_DMXPixelMapping.h"

#include "AssetToolsModule.h"
#include "Misc/CoreDelegates.h"
#include "ThumbnailRendering/ThumbnailManager.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingEditorModule"

EAssetTypeCategories::Type FDMXPixelMappingEditorModule::DMXPixelMappingCategory;
const FName FDMXPixelMappingEditorModule::DMXPixelMappingEditorAppIdentifier(TEXT("PixelMappingApp"));

void FDMXPixelMappingEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShared<FExtensibilityManager>();
	ToolBarExtensibilityManager = MakeShared<FExtensibilityManager>();

	// Register Asset Tools for pixel mapping
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	DMXPixelMappingCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("DMX")), LOCTEXT("DmxCategory", "DMX"));
	RegisterAssetTypeAction(AssetTools, MakeShared<FAssetTypeActions_DMXPixelMapping>());

	FDMXPixelMappingEditorStyle::Initialize();
	FDMXPixelMappingEditorCommands::Register();

	// Any attempt to use GEditor right now will fail as it hasn't been initialized yet. Waiting for post engine init resolves that.
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FDMXPixelMappingEditorModule::OnPostEngineInit);
}

void FDMXPixelMappingEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (TSharedPtr<IAssetTypeActions> Action : CreatedAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action.ToSharedRef());
		}
	}

	FDMXPixelMappingEditorStyle::Shutdown();

	if (UObjectInitialized() && GIsEditor)
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UDMXPixelMapping::StaticClass());
	}

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

const FDMXPixelMappingEditorCommands& FDMXPixelMappingEditorModule::GetCommands() const
{
	return FDMXPixelMappingEditorCommands::Get();
}

void FDMXPixelMappingEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

void FDMXPixelMappingEditorModule::OnPostEngineInit()
{
	if (GIsEditor)
	{
		UThumbnailManager::Get().RegisterCustomRenderer(UDMXPixelMapping::StaticClass(), UDMXPixelMappingThumbnailRendering::StaticClass());
	}
}

IMPLEMENT_MODULE(FDMXPixelMappingEditorModule, DMXPixelMappingEditor)

#undef LOCTEXT_NAMESPACE
