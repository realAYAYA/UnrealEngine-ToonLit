// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FOnlineServicesCommonModule : public IModuleInterface
{
public:
	virtual void StartupModule()
	{

	}

	virtual void ShutdownModule()
	{

	}
};

IMPLEMENT_MODULE(FOnlineServicesCommonModule, OnlineServicesCommon);
