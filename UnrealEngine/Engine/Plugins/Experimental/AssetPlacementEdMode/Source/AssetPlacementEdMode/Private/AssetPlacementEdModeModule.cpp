// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetPlacementEdModeModule.h"
#include "AssetPlacementEdModeCommands.h"
#include "AssetPlacementEdModeStyle.h"
#include "Modules/ModuleManager.h"
#include "AssetTypeActions_PlacementPalette.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "AssetPlacementEdMode"

IMPLEMENT_MODULE( FAssetPlacementEdMode, AssetPlacementEdMode );

#if !UE_IS_COOKED_EDITOR
namespace AssetPlacementEdModeInternal
{
	static int32 GEnableInstanceWorkflows = 1;
	static FAutoConsoleVariableRef CVarEnableInstanceWorkflows(
		TEXT("PlacementMode.EnableInstanceWorkflows"),
		GEnableInstanceWorkflows,
		TEXT("Is support for instances everywhere in placement mode enabled?"),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			// Force a reset on the registered commands to rebuild the modes' set of valid toolbox commands
			FAssetPlacementEdModeCommands::Unregister();
			FAssetPlacementEdModeCommands::Register();
		})
	);
}

namespace AssetPlacementEdModeUtil
{
	bool AreInstanceWorkflowsEnabled()
	{
		return AssetPlacementEdModeInternal::GEnableInstanceWorkflows > 0;
	}
}
#endif // !UE_IS_COOKED_EDITOR

void FAssetPlacementEdMode::StartupModule()
{
	FAssetPlacementEdModeStyle::Get();
	FAssetPlacementEdModeCommands::Register();

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	PaletteAssetActions = MakeShared<FAssetTypeActions_PlacementPalette>(EAssetTypeCategories::Type::Misc);
	AssetToolsModule.Get().RegisterAssetTypeActions(PaletteAssetActions.ToSharedRef());
}

void FAssetPlacementEdMode::ShutdownModule()
{
	if (FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
	{
		AssetToolsModule->Get().UnregisterAssetTypeActions(PaletteAssetActions.ToSharedRef());
	}
	PaletteAssetActions.Reset();

	FAssetPlacementEdModeCommands::Unregister();
	FAssetPlacementEdModeStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE
