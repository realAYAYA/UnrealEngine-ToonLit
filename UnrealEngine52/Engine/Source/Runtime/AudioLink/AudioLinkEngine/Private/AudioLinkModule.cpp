// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AudioLinkLog.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAudioLink);

class FAudioLinkModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override
	{
	}
	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FAudioLinkModule, AudioLink);
