// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeTestEditorModule.h"
#include "InterchangeImportTestPlan.h"
#include "InterchangeImportTestSettings.h"
#include "InterchangeTestsModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "InterchangeTestFunctionLayout.h"
#include "InterchangeTestFunction.h"
#include "AssetTypeActions_InterchangeImportTestPlan.h"
#include "AssetToolsModule.h"


#define LOCTEXT_NAMESPACE "InterchangeTestEditorModule"


FInterchangeTestEditorModule& FInterchangeTestEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FInterchangeTestEditorModule>(INTERCHANGETESTEDITOR_MODULE_NAME);
}


bool FInterchangeTestEditorModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(INTERCHANGETESTEDITOR_MODULE_NAME);
}


void FInterchangeTestEditorModule::StartupModule()
{
	// Register the InterchangeImportTestPlan asset
	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();
	AssetTypeActions = MakeShared<FAssetTypeActions_InterchangeImportTestPlan>();
	AssetTools.RegisterAssetTypeActions(AssetTypeActions.ToSharedRef());

	// Register the FInterchangeTestFunction struct customization
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(FInterchangeTestFunction::StaticStruct()->GetFName(),
	FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInterchangeTestFunctionLayout::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
}


void FInterchangeTestEditorModule::ShutdownModule()
{
	if (UObjectInitialized())
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		IAssetTools& AssetTools = AssetToolsModule.Get();
		AssetTools.UnregisterAssetTypeActions(AssetTypeActions.ToSharedRef());

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FInterchangeTestFunction::StaticStruct()->GetFName());
	}
}


IMPLEMENT_MODULE(FInterchangeTestEditorModule, InterchangeTestEditor);


#undef LOCTEXT_NAMESPACE

