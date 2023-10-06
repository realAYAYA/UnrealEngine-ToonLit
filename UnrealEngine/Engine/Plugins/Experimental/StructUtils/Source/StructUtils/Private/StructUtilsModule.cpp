// Copyright Epic Games, Inc. All Rights Reserved.

#include "StructUtilsModule.h"

#if WITH_EDITORONLY_DATA
#include "InstancedStruct.h"
#endif // WITH_EDITORONLY_DATA

#define LOCTEXT_NAMESPACE "StructUtils"

class FStructUtilsModule : public IStructUtilsModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FStructUtilsModule, StructUtils)

void FStructUtilsModule::StartupModule()
{
	// Called right after the module DLL has been loaded and the module object has been created
	// Load dependent modules here, and they will be guaranteed to be available during ShutdownModule. ie:
	// 		FModuleManager::Get().LoadModuleChecked(TEXT("HTTP"));
	
#if WITH_EDITORONLY_DATA
	UE::StructUtils::Private::RegisterInstancedStructForLocalization();
#endif // WITH_EDITORONLY_DATA
}

void FStructUtilsModule::ShutdownModule()
{
	// Called before the module is unloaded, right before the module object is destroyed.
	// During normal shutdown, this is called in reverse order that modules finish StartupModule().
	// This means that, as long as a module references dependent modules in it's StartupModule(), it
	// can safely reference those dependencies in ShutdownModule() as well.
}

#undef LOCTEXT_NAMESPACE
