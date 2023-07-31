// Copyright Epic Games, Inc. All Rights Reserved.

// Includes
#include "Modules/ModuleManager.h"
#include "INUTUnrealEngine.h"

#include "UnrealEngineEnvironment.h"

/**
 * Module implementation
 */
class FNUTUnrealEngine : public INUTUnrealEngine
{
public:
	virtual void StartupModule() override
	{
		FNUTModuleInterface::StartupModule();

		FShooterGameEnvironment::Register();
		FQAGameEnvironment::Register();
		FUTEnvironment::Register();
	}

	virtual void ShutdownModule() override
	{
		FNUTModuleInterface::ShutdownModule();
	}
};


IMPLEMENT_MODULE(FNUTUnrealEngine, NUTUnrealEngine);

