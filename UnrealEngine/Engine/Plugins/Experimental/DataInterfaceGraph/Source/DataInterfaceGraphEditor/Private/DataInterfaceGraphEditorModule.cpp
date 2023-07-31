// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceGraphEditorModule.h"
#include "IAssetTools.h"
#include "AssetTypeActions_DataInterfaceGraph.h"
#include "DataInterfacePropertyTypeCustomization.h"

namespace UE::DataInterfaceGraphEditor
{

void FModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTypeActions_DataInterfaceGraph = MakeShared<UE::DataInterfaceGraphEditor::FAssetTypeActions_DataInterfaceGraph>();
	AssetTools.RegisterAssetTypeActions(AssetTypeActions_DataInterfaceGraph.ToSharedRef());

	DataInterfacePropertyTypeIdentifier = MakeShared<UE::DataInterfaceGraphEditor::FPropertyTypeIdentifier>();
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FInterfaceProperty::StaticClass()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<UE::DataInterfaceGraphEditor::FDataInterfacePropertyTypeCustomization>(); }),
		DataInterfacePropertyTypeIdentifier);
}

void FModule::ShutdownModule()
{
	if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(AssetTypeActions_DataInterfaceGraph.ToSharedRef());
	}
	
	if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FInterfaceProperty::StaticClass()->GetFName(), DataInterfacePropertyTypeIdentifier);
	}
}

}

IMPLEMENT_MODULE(UE::DataInterfaceGraphEditor::FModule, DataInterfaceGraphEditor);