// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicMaterialModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectBase.h"

DEFINE_LOG_CATEGORY(LogDynamicMaterial);

bool FDynamicMaterialModule::bIsEngineExiting = false;

#if WITH_EDITOR
FDMCreateEditorOnlyDataDelegate FDynamicMaterialModule::CreateEditorOnlyDataDelegate;
#endif

static TAutoConsoleVariable<bool> CVarExportMaterials(
	TEXT("DM.ExportMaterials"),
	false,
	TEXT("If enabled, all materials, including previews, are exported to /Game/DynamicMaterials."),
	ECVF_SetByConsole);

bool FDynamicMaterialModule::IsMaterialExportEnabled()
{
	return CVarExportMaterials.GetValueOnAnyThread();
}

bool FDynamicMaterialModule::AreUObjectsSafe()
{
	return UObjectInitialized() && !bIsEngineExiting;
}

FDynamicMaterialModule& FDynamicMaterialModule::Get()
{
	return FModuleManager::LoadModuleChecked<FDynamicMaterialModule>("DynamicMaterial");
}

void FDynamicMaterialModule::StartupModule()
{
	EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddStatic(&FDynamicMaterialModule::HandleEnginePreExit);
}

void FDynamicMaterialModule::ShutdownModule()
{
	if (EnginePreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);
	}
}

void FDynamicMaterialModule::HandleEnginePreExit()
{
	bIsEngineExiting = true;
}

#if WITH_EDITOR
TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface> FDynamicMaterialModule::CreateEditorOnlyData(UDynamicMaterialModel* InMaterialModel)
{
	if (CreateEditorOnlyDataDelegate.IsBound())
	{
		return CreateEditorOnlyDataDelegate.Execute(InMaterialModel);
	}

	return nullptr;
}
#endif

IMPLEMENT_MODULE(FDynamicMaterialModule, DynamicMaterial)
