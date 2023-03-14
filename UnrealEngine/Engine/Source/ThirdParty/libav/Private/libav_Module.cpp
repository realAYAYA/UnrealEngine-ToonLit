// Copyright Epic Games, Inc. All Rights Reserved.

#include "libav_Module.h"
#include "Modules/ModuleManager.h"

class ILibAvModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
	}
};

IMPLEMENT_MODULE(ILibAvModule, libav);
