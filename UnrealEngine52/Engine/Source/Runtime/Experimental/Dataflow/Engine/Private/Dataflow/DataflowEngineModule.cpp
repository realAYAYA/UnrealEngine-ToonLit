// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "DataflowEngineLogPrivate.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * The public interface to this module
 */
class FDataflowEngineModule : public IModuleInterface
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

IMPLEMENT_MODULE( FDataflowEngineModule, DataflowEngine )



