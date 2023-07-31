// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "MassSignalTypes.h"
#include "IMassSignalsModule.h"

DEFINE_LOG_CATEGORY(LogMassSignals)


class FMassSignalsModule : public IMassSignalsModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FMassSignalsModule, MassSignals)



void FMassSignalsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FMassSignalsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}



