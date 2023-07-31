// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "Online/OnlineServicesOSSAdapter.h"
#include "Online/OnlineServicesRegistry.h"
#include "Online/OnlineIdOSSAdapter.h"
#include "Online/SessionsOSSAdapter.h"

#include "OnlineSubsystem.h"

namespace UE::Online
{

class FOnlineServicesOSSAdapterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
protected:
};

struct FOSSAdapterService
{
	EOnlineServices Service = EOnlineServices::Default;
	FString ConfigName;
	FName OnlineSubsystem;
	int Priority = -1;
};

struct FOSSAdapterConfig
{
	TArray<FOSSAdapterService> Services;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FOSSAdapterService)
	ONLINE_STRUCT_FIELD(FOSSAdapterService, Service),
	ONLINE_STRUCT_FIELD(FOSSAdapterService, ConfigName),
	ONLINE_STRUCT_FIELD(FOSSAdapterService, OnlineSubsystem),
	ONLINE_STRUCT_FIELD(FOSSAdapterService, Priority)
END_ONLINE_STRUCT_META()

BEGIN_ONLINE_STRUCT_META(FOSSAdapterConfig)
	ONLINE_STRUCT_FIELD(FOSSAdapterConfig, Services)
END_ONLINE_STRUCT_META()

/* Meta */ }

class FOnlineServicesFactoryOSSAdapter : public IOnlineServicesFactory
{
public:
	FOnlineServicesFactoryOSSAdapter(const FOSSAdapterService& InConfig)
		: Config(InConfig)
	{
	}

	virtual ~FOnlineServicesFactoryOSSAdapter() {}
	virtual TSharedPtr<IOnlineServices> Create(FName InInstanceName) override
	{
		IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(Config.OnlineSubsystem);
		if (Subsystem != nullptr)
		{
			return MakeShared<FOnlineServicesOSSAdapter>(Config.Service, Config.ConfigName, InInstanceName, Subsystem);
		}
		else
		{
			return nullptr;
		}
	}
protected:
	FOSSAdapterService Config;
};

void FOnlineServicesOSSAdapterModule::StartupModule()
{
	FOnlineConfigProviderGConfig ConfigProvider(GEngineIni);
	FOSSAdapterConfig Config;
	if (LoadConfig(ConfigProvider, TEXT("OnlineServices.OSSAdapter"), Config))
	{
		for (const FOSSAdapterService& ServiceConfig : Config.Services)
		{
			if (IOnlineSubsystem::IsEnabled(ServiceConfig.OnlineSubsystem))
			{
				FOnlineServicesRegistry::Get().RegisterServicesFactory(ServiceConfig.Service, MakeUnique<FOnlineServicesFactoryOSSAdapter>(ServiceConfig), ServiceConfig.Priority);
				FOnlineIdRegistryRegistry::Get().RegisterAccountIdRegistry(ServiceConfig.Service, new FOnlineAccountIdRegistryOSSAdapter(ServiceConfig.Service), ServiceConfig.Priority);
				FOnlineIdRegistryRegistry::Get().RegisterSessionIdRegistry(ServiceConfig.Service, new FOnlineSessionIdRegistryOSSAdapter(ServiceConfig.Service), ServiceConfig.Priority);
				FOnlineIdRegistryRegistry::Get().RegisterSessionInviteIdRegistry(ServiceConfig.Service, new FOnlineSessionInviteIdRegistryOSSAdapter(ServiceConfig.Service), ServiceConfig.Priority);
			}
		}
	}
}

void FOnlineServicesOSSAdapterModule::ShutdownModule()
{
	FOnlineConfigProviderGConfig ConfigProvider(GEngineIni);
	FOSSAdapterConfig Config;
	if (LoadConfig(ConfigProvider, TEXT("OnlineServices.OSSAdapter"), Config))
	{
		for (const FOSSAdapterService& ServiceConfig : Config.Services)
		{
			if (IOnlineSubsystem::IsEnabled(ServiceConfig.OnlineSubsystem))
			{
				FOnlineServicesRegistry::Get().UnregisterServicesFactory(ServiceConfig.Service, ServiceConfig.Priority);
				FOnlineIdRegistryRegistry::Get().UnregisterAccountIdRegistry(ServiceConfig.Service, ServiceConfig.Priority);
				FOnlineIdRegistryRegistry::Get().UnregisterSessionIdRegistry(ServiceConfig.Service, ServiceConfig.Priority);
				FOnlineIdRegistryRegistry::Get().UnregisterSessionInviteIdRegistry(ServiceConfig.Service, ServiceConfig.Priority);
			}
		}
	}
}

/* UE::Online */ }

IMPLEMENT_MODULE(UE::Online::FOnlineServicesOSSAdapterModule, OnlineServicesOSSAdapter);
