// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorInteractiveToolsFrameworkModule.h"

#include "ComponentSourceInterfaces.h"
#include "Modules/ModuleManager.h"
#include "Templates/UniquePtr.h"
#include "Tools/EditorComponentSourceFactory.h"

#define LOCTEXT_NAMESPACE "FEditorInteractiveToolsFrameworkModule"

int32 FEditorInteractiveToolsFrameworkGlobals::RegisteredStaticMeshTargetFactoryKey = -1;

void FEditorInteractiveToolsFrameworkModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FEditorInteractiveToolsFrameworkGlobals::RegisteredStaticMeshTargetFactoryKey 
		= AddComponentTargetFactory( TUniquePtr<FComponentTargetFactory>{new FStaticMeshComponentTargetFactory{} } );

	// The Interactive Tools Framework needs to be explicitly loaded, at the very least to make sure that
	// the InteractiveToolsSelectionStoreSubsystem gets initialized.
	FModuleManager::Get().LoadModule(TEXT("InteractiveToolsFramework"));
}

void FEditorInteractiveToolsFrameworkModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEditorInteractiveToolsFrameworkModule, EditorInteractiveToolsFramework)
