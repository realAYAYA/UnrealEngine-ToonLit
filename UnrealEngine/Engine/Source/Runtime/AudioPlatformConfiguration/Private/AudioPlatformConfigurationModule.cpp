// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioPlatformConfigurationModule.h"
#include "Modules/ModuleManager.h"

class FAudioPlatformConfigurationModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
	}
};

IMPLEMENT_MODULE(FAudioPlatformConfigurationModule, AudioPlatformConfiguration);