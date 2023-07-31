// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserEditorModule.h"
#include "IAssetTools.h"
#include "AssetTypeActions_Chooser.h"
#include "ChooserTableEditor.h"

namespace UE::ChooserEditor
{

void FModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTypeActions_ChooserTable = MakeShared<FAssetTypeActions_ChooserTable>();
	AssetTools.RegisterAssetTypeActions(AssetTypeActions_ChooserTable.ToSharedRef());
	FChooserTableEditor::RegisterWidgets();
}

void FModule::ShutdownModule()
{
	if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(AssetTypeActions_ChooserTable.ToSharedRef());
	}
}

}

IMPLEMENT_MODULE(UE::ChooserEditor::FModule, ChooserEditor);