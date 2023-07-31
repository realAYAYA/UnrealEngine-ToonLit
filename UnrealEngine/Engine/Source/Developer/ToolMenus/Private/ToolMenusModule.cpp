// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IToolMenusModule.h"


/**
 * Implements the Tool menus module.
 */
class FToolMenusModule
	: public IToolMenusModule
{
public:

	// IToolMenusModule interface

	virtual void StartupModule( ) override
	{
	}

	virtual void ShutdownModule( ) override
	{
	}

	// End IToolInterface interface
};


IMPLEMENT_MODULE(FToolMenusModule, ToolMenus)
