// Copyright Epic Games, Inc. All Rights Reserved.

// #include "ToolkitStyle.h"

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

// @TODO: ~enable ToolkitStyle set 

class FWidgetRegistrationModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface
	
	virtual void StartupModule() override;
	
	virtual void ShutdownModule() override;

};


IMPLEMENT_MODULE(FWidgetRegistrationModule, WidgetRegistration);
