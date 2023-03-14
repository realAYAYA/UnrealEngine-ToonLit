// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FOnlineServicesCommonEngineUtilsModule : public IModuleInterface
{
public:
	virtual void StartupModule()
	{

	}

	virtual void ShutdownModule()
	{

	}
};

IMPLEMENT_MODULE(FOnlineServicesCommonEngineUtilsModule, OnlineServicesCommonEngineUtils);