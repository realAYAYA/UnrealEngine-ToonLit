// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServicesRegistry.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineServicesDelegates.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/LazySingleton.h"

namespace UE::Online {

FOnlineServicesRegistry& FOnlineServicesRegistry::Get()
{
	return TLazySingleton<FOnlineServicesRegistry>::Get();
}

void FOnlineServicesRegistry::TearDown()
{
	return TLazySingleton<FOnlineServicesRegistry>::TearDown();
}

EOnlineServices FOnlineServicesRegistry::ResolveServiceName(EOnlineServices OnlineServices) const
{
	if (OnlineServices == EOnlineServices::Default)
	{
		if (DefaultServiceOverride != EOnlineServices::Default)
		{
			OnlineServices = DefaultServiceOverride;
		}
		else
		{
			FString Value;

			if (GConfig->GetString(TEXT("OnlineServices"), TEXT("DefaultServices"), Value, GEngineIni))
			{
				LexFromString(OnlineServices, *Value);
			}
		};
	}
	else if (OnlineServices == EOnlineServices::Platform)
	{
		FString Value;

		if (GConfig->GetString(TEXT("OnlineServices"), TEXT("PlatformServices"), Value, GEngineIni))
		{
			LexFromString(OnlineServices, *Value);
		}
	}

	return OnlineServices;
}

void FOnlineServicesRegistry::RegisterServicesFactory(EOnlineServices OnlineServices, TUniquePtr<IOnlineServicesFactory>&& Factory, int32 Priority)
{
	OnlineServices = ResolveServiceName(OnlineServices);

	FFactoryAndPriority* ExistingFactoryAndPriority = ServicesFactories.Find(OnlineServices);
	if (ExistingFactoryAndPriority == nullptr || ExistingFactoryAndPriority->Priority < Priority)
	{
		ServicesFactories.Add(OnlineServices, FFactoryAndPriority(MoveTemp(Factory), Priority));
	}
}

void FOnlineServicesRegistry::UnregisterServicesFactory(EOnlineServices OnlineServices, int32 Priority)
{
	OnlineServices = ResolveServiceName(OnlineServices);

	FFactoryAndPriority* ExistingFactoryAndPriority = ServicesFactories.Find(OnlineServices);
	if (ExistingFactoryAndPriority != nullptr && ExistingFactoryAndPriority->Priority == Priority)
	{
		ServicesFactories.Remove(OnlineServices);
	}

	DestroyAllNamedServicesInstances(OnlineServices);
}

bool FOnlineServicesRegistry::IsLoaded(EOnlineServices OnlineServices, FName InstanceName) const
{
	OnlineServices = ResolveServiceName(OnlineServices);

	bool bExists = false;
	if (const TMap<FName, TSharedRef<IOnlineServices>>* OnlineServicesInstances = NamedServiceInstances.Find(OnlineServices))
	{
		bExists = OnlineServicesInstances->Find(InstanceName) != nullptr;
	}
	return bExists;
}

TSharedPtr<IOnlineServices> FOnlineServicesRegistry::GetNamedServicesInstance(EOnlineServices OnlineServices, FName InstanceName)
{
	OnlineServices = ResolveServiceName(OnlineServices);

	TSharedPtr<IOnlineServices> Services;

	if (OnlineServices < EOnlineServices::None)
	{
		if (TSharedRef<IOnlineServices>* ServicesPtr = NamedServiceInstances.FindOrAdd(OnlineServices).Find(InstanceName))
		{
			Services = *ServicesPtr;
		}
		else
		{
			Services = CreateServices(OnlineServices, InstanceName);
			if (Services.IsValid())
			{
				NamedServiceInstances.FindOrAdd(OnlineServices).Add(InstanceName, Services.ToSharedRef());
				OnOnlineServicesCreated.Broadcast(Services.ToSharedRef());
			}
		}
	}

	return Services;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOnlineServicesRegistry::SetDefaultServiceOverride(EOnlineServices DefaultService)
{
	// No need to call ResolveServiceName here as a generic services name can be used as a Default Service Override
	DefaultServiceOverride = DefaultService;
}

void FOnlineServicesRegistry::ClearDefaultServiceOverride()
{
	DefaultServiceOverride = EOnlineServices::Default;
}
#endif //WITH_DEV_AUTOMATION_TESTS

void FOnlineServicesRegistry::DestroyNamedServicesInstance(EOnlineServices OnlineServices, FName InstanceName)
{
	OnlineServices = ResolveServiceName(OnlineServices);

	if (TSharedRef<IOnlineServices>* ServicesPtr = NamedServiceInstances.FindOrAdd(OnlineServices).Find(InstanceName))
	{
		(*ServicesPtr)->Destroy();

		NamedServiceInstances.FindOrAdd(OnlineServices).Remove(InstanceName);
	}
}

void FOnlineServicesRegistry::DestroyAllNamedServicesInstances(EOnlineServices OnlineServices)
{
	OnlineServices = ResolveServiceName(OnlineServices);

	if (TMap<FName, TSharedRef<IOnlineServices>>* ServicesMapPtr = NamedServiceInstances.Find(OnlineServices))
	{
		for (const TPair<FName, TSharedRef<IOnlineServices>>& ServicesEntryRef : *ServicesMapPtr)
		{
			ServicesEntryRef.Value->Destroy();
		}

		NamedServiceInstances.Remove(OnlineServices);
	}
}

TSharedPtr<IOnlineServices> FOnlineServicesRegistry::CreateServices(EOnlineServices OnlineServices, FName InstanceName)
{
	OnlineServices = ResolveServiceName(OnlineServices);

	TSharedPtr<IOnlineServices> Services;

	FFactoryAndPriority* FactoryAndPriority = ServicesFactories.Find(OnlineServices);
	if (FactoryAndPriority != nullptr)
	{
		Services = FactoryAndPriority->Factory->Create(InstanceName);
		Services->Init();
	}

	return Services;
}

void FOnlineServicesRegistry::GetAllServicesInstances(TArray<TSharedRef<IOnlineServices>>& OutOnlineServices) const
{
	for (const TPair<EOnlineServices, TMap<FName, TSharedRef<IOnlineServices>>>& OnlineServiceTypesMaps : NamedServiceInstances)
	{
		for (const TPair<FName, TSharedRef<IOnlineServices>>& NamedInstance : OnlineServiceTypesMaps.Value)
		{
			OutOnlineServices.Emplace(NamedInstance.Value);
		}
	}
}

FOnlineServicesRegistry::~FOnlineServicesRegistry()
{
	for (TPair<EOnlineServices, TMap<FName, TSharedRef<IOnlineServices>>>& ServiceInstances : NamedServiceInstances)
	{
		for (TPair<FName, TSharedRef<IOnlineServices>>& ServiceInstance : ServiceInstances.Value)
		{
			ServiceInstance.Value->Destroy();
		}
	}
}

/* UE::Online */ }
