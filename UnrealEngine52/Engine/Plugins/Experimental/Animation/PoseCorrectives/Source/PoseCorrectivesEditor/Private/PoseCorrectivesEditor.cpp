// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesEditor.h"
#include "PoseCorrectivesCommands.h"
#include "AssetTypeActions_PoseCorrectives.h"
#include "CorrectivesEditMode.h"
#include "EditorModeRegistry.h"

#define LOCTEXT_NAMESPACE "PoseCorrectivesEidtor"


void FPoseCorrectivesEditorModule::StartupModule()
{
	FPoseCorrectivesCommands::Register();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	PoseCorrectivesAssetActions = MakeShareable(new FAssetTypeActions_PoseCorrectives());
	AssetTools.RegisterAssetTypeActions(PoseCorrectivesAssetActions.ToSharedRef());

	FEditorModeRegistry::Get().RegisterMode<FCorrectivesEditMode>(
        FCorrectivesEditMode::ModeName,
        LOCTEXT("CorrectivesEditMode", "PoseCorrectives"),
        FSlateIcon(),
        false);
}

void FPoseCorrectivesEditorModule::ShutdownModule()
{
	FPoseCorrectivesCommands::Unregister();

	if (PoseCorrectivesAssetActions.IsValid())
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(PoseCorrectivesAssetActions.ToSharedRef());
		}
		PoseCorrectivesAssetActions.Reset();
	}
	
    FEditorModeRegistry::Get().UnregisterMode(FCorrectivesEditMode::ModeName);
}
	
IMPLEMENT_MODULE(FPoseCorrectivesEditorModule, PoseCorrectivesEditor)

#undef LOCTEXT_NAMESPACE
