// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

class FDeveloperSettingsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

void FDeveloperSettingsModule::StartupModule()
{
}

void FDeveloperSettingsModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDeveloperSettingsModule, DeveloperSettings );
