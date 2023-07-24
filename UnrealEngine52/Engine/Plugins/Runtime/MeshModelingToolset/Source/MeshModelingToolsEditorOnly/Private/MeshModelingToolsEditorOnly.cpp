// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshModelingToolsEditorOnly.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#include "DetailsCustomizations/UVUnwrapToolCustomizations.h"
#include "Properties/RecomputeUVsProperties.h"

#define LOCTEXT_NAMESPACE "FMeshModelingToolsEditorOnlyModule"

void FMeshModelingToolsEditorOnlyModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FMeshModelingToolsEditorOnlyModule::OnPostEngineInit);

}

void FMeshModelingToolsEditorOnlyModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	// Unregister customizations
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (const FName& ClassName : ClassesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassName);
		}
		for (const FName& PropertyName : PropertiesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(PropertyName);
		}
	}
}

void FMeshModelingToolsEditorOnlyModule::OnPostEngineInit()
{

	// Register details view customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	/// Unwrap
	PropertyModule.RegisterCustomClassLayout("RecomputeUVsToolProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FRecomputeUVsToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(URecomputeUVsToolProperties::StaticClass()->GetFName());


}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMeshModelingToolsEditorOnlyModule, MeshModelingToolsEditorOnly)