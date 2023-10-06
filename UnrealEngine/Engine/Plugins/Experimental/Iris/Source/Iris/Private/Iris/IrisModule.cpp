// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FIrisModule : public IModuleInterface
{
private:
	virtual void StartupModule() override
	{
#if UE_WITH_IRIS
		FModuleManager::Get().LoadModule("IrisCore", ELoadModuleFlags::None);
#endif
	}
};

IMPLEMENT_MODULE(FIrisModule, Iris);
