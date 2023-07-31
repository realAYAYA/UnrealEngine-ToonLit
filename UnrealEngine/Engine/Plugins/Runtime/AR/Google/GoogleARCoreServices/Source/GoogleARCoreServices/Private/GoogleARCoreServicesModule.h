// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FGoogleARCoreServicesManager;
class FGoogleARCoreServicesModule : public IModuleInterface
{
public:

	// This function will always return a valid FGoogleARCoreServicesManager.
	// Or it will cause a crash if the function is called before the module startup.
	static FGoogleARCoreServicesManager* GetARCoreServicesManager();

	/** IModuleInterface implementation. */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};
