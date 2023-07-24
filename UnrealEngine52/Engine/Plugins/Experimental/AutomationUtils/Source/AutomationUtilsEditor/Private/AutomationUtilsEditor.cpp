// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "AutomationUtilsEditor"

class FAutomationUtilsEditorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{

	}
	
	virtual void ShutdownModule() override
	{

	}
};

IMPLEMENT_MODULE( FAutomationUtilsEditorModule, AutomationUtilsEditor);

#undef LOCTEXT_NAMESPACE
