// Copyright Epic Games, Inc. All Rights Reserved.
#include "AutomationTestModule.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


DEFINE_LOG_CATEGORY(LogAutomationTest)

/**
 * Implements the AutomationTesting module.
 */
class FAutomationTestModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FAutomationTestModule, AutomationTest);
