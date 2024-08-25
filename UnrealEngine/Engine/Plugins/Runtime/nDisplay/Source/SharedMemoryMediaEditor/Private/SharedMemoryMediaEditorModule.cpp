// Copyright Epic Games, Inc. All Rights Reserved.

#include "SharedMemoryMediaEditorModule.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

#include "ModularFeatures/SharedMemoryMediaInitializerFeature.h"


void FSharedMemoryMediaEditorModule::StartupModule()
{
	RegisterModularFeatures();
}

void FSharedMemoryMediaEditorModule::ShutdownModule()
{
	UnregisterModularFeatures();
}

void FSharedMemoryMediaEditorModule::RegisterModularFeatures()
{
	// Instantiate modular features
	MediaInitializer = MakeUnique<FSharedMemoryMediaInitializerFeature>();

	// Register modular features
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ModularFeatures.RegisterModularFeature(FSharedMemoryMediaInitializerFeature::ModularFeatureName, MediaInitializer.Get());
}

void FSharedMemoryMediaEditorModule::UnregisterModularFeatures()
{
	// Unregister modular features
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	ModularFeatures.UnregisterModularFeature(FSharedMemoryMediaInitializerFeature::ModularFeatureName, MediaInitializer.Get());
}


IMPLEMENT_MODULE(FSharedMemoryMediaEditorModule, SharedMemoryMediaEditor)
