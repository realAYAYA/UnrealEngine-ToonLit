// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Online/OnlineServicesRegistry.h"

class FOnlineServicesInterfaceModule : public IModuleInterface
{
public:
private:
	virtual void ShutdownModule() override
	{
		UE::Online::FOnlineServicesRegistry::TearDown();
	}
};

IMPLEMENT_MODULE(FOnlineServicesInterfaceModule, OnlineServicesInterface);
