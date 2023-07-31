// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeFrameworkEditorModule.h"
#include "ComputeFramework/ComputeFrameworkCompilationTick.h"

#include "ComputeFramework/ComputeKernelAssetActions.h"
#include "ComputeFramework/ComputeKernelFromTextAssetActions.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"

void FComputeFrameworkEditorModule::StartupModule()
{
	TickObject = MakeUnique<FComputeFrameworkCompilationTick>();

// 	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
// 	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ComputeKernel));
// 	AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ComputeKernelFromText));
}

void FComputeFrameworkEditorModule::ShutdownModule()
{
	TickObject = nullptr;
}

IMPLEMENT_MODULE(FComputeFrameworkEditorModule, ComputeFrameworkEditor)
