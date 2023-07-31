// Copyright Epic Games, Inc. All Rights Reserved.


#include "Chaos/ChaosSolverEditorPlugin.h"

#include "AssetToolsModule.h"
#include "CoreMinimal.h"
#include "Chaos/ChaosSolver.h"
#include "Chaos/AssetTypeActions_ChaosSolver.h"
#include "Chaos/ChaosSolverEditorStyle.h"
#include "Chaos/ChaosSolverEditorCommands.h"
#include "HAL/ConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ChaosSolverEditorDetails.h"



IMPLEMENT_MODULE( IChaosSolverEditorPlugin, ChaosSolverEditor )

void IChaosSolverEditorPlugin::StartupModule()
{
	FChaosSolverEditorStyle::Get();

	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();
	AssetTypeActions_ChaosSolver = new FAssetTypeActions_ChaosSolver();
	AssetTools.RegisterAssetTypeActions(MakeShareable(AssetTypeActions_ChaosSolver));

	if (GIsEditor && !IsRunningCommandlet())
	{
	}

	// Register details view customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("ChaosDebugSubstepControl", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FChaosDebugSubstepControlCustomization::MakeInstance));
}


void IChaosSolverEditorPlugin::ShutdownModule()
{
	if (UObjectInitialized())
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		IAssetTools& AssetTools = AssetToolsModule.Get();
		AssetTools.UnregisterAssetTypeActions(AssetTypeActions_ChaosSolver->AsShared());
	}

	// Unregister details view customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomPropertyTypeLayout("ChaosDebugSubstepControl");
}



