// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLWITypes.h"
#include "IMassLWIModule.h"

DEFINE_LOG_CATEGORY(LogMassLWI)


class FMassLWIModule : public IMassLWIModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FMassLWIModule, MassLWI)



void FMassLWIModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FMassLWIModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}



