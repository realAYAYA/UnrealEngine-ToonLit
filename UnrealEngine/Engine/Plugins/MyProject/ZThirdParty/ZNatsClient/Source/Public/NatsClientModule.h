#pragma once

#include "Modules/ModuleManager.h"
#include "CoreMinimal.h"
#include "NatsClient.h"

class ZNATSCLIENT_API INatsClientModule : public IModuleInterface
{
public:
	static INatsClientModule& Get()
	{
		return FModuleManager::LoadModuleChecked<INatsClientModule>("ZNatsClient");
	}

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("ZNatsClient");
	}

	virtual FNatsClientPtr CreateNatsClient() = 0;
	
};

DECLARE_LOG_CATEGORY_EXTERN(LogZNats, Log, All);