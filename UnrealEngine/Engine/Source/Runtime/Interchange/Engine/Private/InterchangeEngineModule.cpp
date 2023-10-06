// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include "InterchangeEngineLogPrivate.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogInterchangeEngine);


/**
 * Implements the Interchange module.
 */
class FInterchangeEngineModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};


IMPLEMENT_MODULE(FInterchangeEngineModule, InterchangeEngine);
