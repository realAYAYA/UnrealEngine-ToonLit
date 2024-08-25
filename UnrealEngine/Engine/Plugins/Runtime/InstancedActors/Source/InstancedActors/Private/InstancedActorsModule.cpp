// Copyright Epic Games, Inc. All Rights Reserved.

#include "IInstancedActorsModule.h"


class FInstancedActorsModuleModule : public IInstancedActorsModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FInstancedActorsModuleModule, InstancedActors)



void FInstancedActorsModuleModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FInstancedActorsModuleModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

