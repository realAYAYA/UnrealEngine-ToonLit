// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceGraphEditorModule.h"
#include "IAssetTools.h"
#include "AssetTypeActions_AnimNextInterfaceGraph.h"
#include "AnimNextInterfacePropertyTypeCustomization.h"

namespace UE::AnimNext::InterfaceGraphEditor
{

void FModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTypeActions_AnimNextInterfaceGraph = MakeShared<UE::AnimNext::InterfaceGraphEditor::FAssetTypeActions_AnimNextInterfaceGraph>();
	AssetTools.RegisterAssetTypeActions(AssetTypeActions_AnimNextInterfaceGraph.ToSharedRef());

	AnimNextInterfacePropertyTypeIdentifier = MakeShared<UE::AnimNext::InterfaceGraphEditor::FPropertyTypeIdentifier>();
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FInterfaceProperty::StaticClass()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<UE::AnimNext::InterfaceGraphEditor::FAnimNextInterfacePropertyTypeCustomization>(); }),
		AnimNextInterfacePropertyTypeIdentifier);
}

void FModule::ShutdownModule()
{
	if(FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(AssetTypeActions_AnimNextInterfaceGraph.ToSharedRef());
	}
	
	if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FInterfaceProperty::StaticClass()->GetFName(), AnimNextInterfacePropertyTypeIdentifier);
	}
}

}

IMPLEMENT_MODULE(UE::AnimNext::InterfaceGraphEditor::FModule, AnimNextInterfaceGraphEditor);