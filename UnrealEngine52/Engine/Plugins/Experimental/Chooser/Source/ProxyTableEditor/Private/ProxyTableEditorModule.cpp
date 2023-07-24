// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProxyTableEditorModule.h"
#include "IAssetTools.h"
#include "AssetTypeActions_ProxyTable.h"
#include "ProxyTableEditor.h"

#define LOCTEXT_NAMESPACE "ChooserEditorModule"

namespace UE::ProxyTableEditor
{

void FModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	AssetTypeActions_ProxyTable = MakeShared<FAssetTypeActions_ProxyTable>();
	AssetTools.RegisterAssetTypeActions(AssetTypeActions_ProxyTable.ToSharedRef());
	FProxyTableEditor::RegisterWidgets();
}
		 

void FModule::ShutdownModule()
{
	if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(AssetTypeActions_ProxyTable.ToSharedRef());
	}
}

}

IMPLEMENT_MODULE(UE::ProxyTableEditor::FModule, ProxyTableEditor);

#undef LOCTEXT_NAMESPACE