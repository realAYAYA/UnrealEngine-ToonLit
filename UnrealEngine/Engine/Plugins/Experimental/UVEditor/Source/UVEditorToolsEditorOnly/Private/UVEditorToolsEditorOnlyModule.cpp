// Copyright Epic Games, Inc. All Rights Reserved.

#include "UVEditorToolsEditorOnlyModule.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#include "DetailsCustomizations/UVUnwrapToolCustomizations.h"
#include "DetailsCustomizations/UVTransformToolCustomizations.h"

#include "Operators/UVEditorRecomputeUVsOp.h"
#include "UVEditorTransformTool.h"
#include "Operators/UVEditorUVTransformOp.h"

#define LOCTEXT_NAMESPACE "FUVEditorToolsEditorOnlyModule"

void FUVEditorToolsEditorOnlyModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FUVEditorToolsEditorOnlyModule::OnPostEngineInit);

}

void FUVEditorToolsEditorOnlyModule::ShutdownModule()
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

void FUVEditorToolsEditorOnlyModule::OnPostEngineInit()
{

	// Register details view customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	/// Unwrap
	PropertyModule.RegisterCustomClassLayout("UVEditorRecomputeUVsToolProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FUVEditorRecomputeUVsToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UUVEditorRecomputeUVsToolProperties::StaticClass()->GetFName());

	// Transform
	PropertyModule.RegisterCustomClassLayout(UUVEditorUVTransformProperties::StaticClass()->GetFName(),
		                                     FOnGetDetailCustomizationInstance::CreateStatic(&FUVEditorUVTransformToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UUVEditorUVTransformProperties::StaticClass()->GetFName());

	// Quick Transform
	PropertyModule.RegisterCustomClassLayout(UUVEditorUVQuickTransformProperties::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FUVEditorUVQuickTransformToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UUVEditorUVQuickTransformProperties::StaticClass()->GetFName());

	// Align
	PropertyModule.RegisterCustomClassLayout(UUVEditorUVAlignProperties::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FUVEditorUVAlignToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UUVEditorUVAlignProperties::StaticClass()->GetFName());

	// Distribute
	PropertyModule.RegisterCustomClassLayout(UUVEditorUVDistributeProperties::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FUVEditorUVDistributeToolDetails::MakeInstance));
	ClassesToUnregisterOnShutdown.Add(UUVEditorUVDistributeProperties::StaticClass()->GetFName());


}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUVEditorToolsEditorOnlyModule, UVEditorToolsEditorOnly)
