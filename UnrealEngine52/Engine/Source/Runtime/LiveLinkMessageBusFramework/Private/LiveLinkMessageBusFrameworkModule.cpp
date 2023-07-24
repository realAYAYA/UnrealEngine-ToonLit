// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FLiveLinkMessageBusFrameworkModule : public IModuleInterface
{
public:

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};

IMPLEMENT_MODULE(FLiveLinkMessageBusFrameworkModule, LiveLinkMessageBusFramework);
