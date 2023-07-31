// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshModelingToolsEditorOnly.h"

#define LOCTEXT_NAMESPACE "FMeshModelingToolsEditorOnlyModule"

void FMeshModelingToolsEditorOnlyModule::StartupModule()
{
}

void FMeshModelingToolsEditorOnlyModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMeshModelingToolsEditorOnlyModule, MeshModelingToolsEditorOnly)
