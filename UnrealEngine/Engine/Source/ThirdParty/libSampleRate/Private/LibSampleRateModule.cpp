// Copyright Epic Games, Inc. All Rights Reserved.

#include "LibSampleRateModule.h"
#include "Modules/ModuleManager.h"

class FLibSampleRateModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
	}
};

IMPLEMENT_MODULE(FLibSampleRateModule, UELibSampleRate);