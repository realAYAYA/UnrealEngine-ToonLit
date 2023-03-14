// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

#include "Online/OnlineServices.h"

class FLazySingleton;

namespace UE::Online {
	
enum class EOnlineServices : uint8;

class ONLINESERVICESINTERFACE_API IOnlineServicesFactory
{
public:
	/**
	 * Virtual destructor
	 */
	virtual ~IOnlineServicesFactory() {}

	/**
	 * Create an IOnlineServices instance
	 *
	 * @return Initialized IOnlineServices instance
	 */
	virtual TSharedPtr<IOnlineServices> Create(FName InstanceName) = 0;
};

class FOnlineServicesRegistry
{
public:
	/**
	 * Get the FOnlineServicesRegistry singleton
	 * 
	 * @return The FOnlineServicesRegistry singleton instance
	 */
	ONLINESERVICESINTERFACE_API static FOnlineServicesRegistry& Get();

	/**
	 * Tear down the singleton instance
	 */
	ONLINESERVICESINTERFACE_API static void TearDown();

	/**
	 * Register a factory for creation of IOnlineServices instances
	 * 
	 * @param OnlineServices Services that the factory is for
	 * @param Factory Factory for creation of IOnlineServices instances
	 * @param Priority Integer priority, allows an existing OnlineServices implementation to be extended and registered with a higher priority so it is used instead
	 */
	ONLINESERVICESINTERFACE_API void RegisterServicesFactory(EOnlineServices OnlineServices, TUniquePtr<IOnlineServicesFactory>&& Factory, int32 Priority = 0);

	/**
	 * Unregister a previously registered IOnlineServices factory
	 *
	 * @param OnlineServices Services that the factory is for
	 * @param Priority Integer priority, will be unregistered only if the priority matches the one that is registered
	 */
	ONLINESERVICESINTERFACE_API void UnregisterServicesFactory(EOnlineServices OnlineServices, int32 Priority = 0);

	/**
	 * Check if an online service instance is loaded
	 *
	 * @param OnlineServices Type of online services for the IOnlineServices instance
	 * @param InstanceName Name of the instance
	 *
	 * @return true if the instance is loaded
	 */
	ONLINESERVICESINTERFACE_API bool IsLoaded(EOnlineServices OnlineServices, FName InstanceName) const;

	/**
	 * Get a named instance of a specific IOnlineServices
	 * 
	 * @param OnlineServices Type of online services for the IOnlineServices instance
	 * @param InstanceName Name of the instance
	 * 
	 * @return The services instance, or an invalid pointer if the OnlineServices is unavailable
	 */
	ONLINESERVICESINTERFACE_API TSharedPtr<IOnlineServices> GetNamedServicesInstance(EOnlineServices OnlineServices, FName InstanceName);

	/**
	 * Destroy a named instance of a specific OnlineServices
	 *
	 * @param OnlineServices  Type of online services for the IOnlineServices instance
	 * @param InstanceName Name of the instance
	 */
	ONLINESERVICESINTERFACE_API void DestroyNamedServicesInstance(EOnlineServices OnlineServices, FName InstanceName);

	/**
	 * Destroy all instances of a specific OnlineServices
	 *
	 * @param OnlineServices  Type of online services for the IOnlineServices instance
	 */
	ONLINESERVICESINTERFACE_API void DestroyAllNamedServicesInstances(EOnlineServices OnlineServices);

	/**
	 * Create and initialize a new IOnlineServices instance
	 *
	 * @param OnlineServices Type of online services for the IOnlineServices instance
	 * 
	 * @return The initialized IOnlineServices instance, or an invalid pointer if the OnlineServices is unavailable
	 */
	ONLINESERVICESINTERFACE_API TSharedPtr<IOnlineServices> CreateServices(EOnlineServices OnlineServices, FName InstanceName);

	/**
	 * Get list of all instantiated OnlineServices
	 * 
	 * @param OutOnlineServices Array of online services to fill
	 */
	ONLINESERVICESINTERFACE_API void GetAllServicesInstances(TArray<TSharedRef<IOnlineServices>>& OutOnlineServices) const;

#if WITH_DEV_AUTOMATION_TESTS
	/**
	 * Adds a temporary override to the default online service. Used for testing to quickly run tests with different default online services.
	 * 
	 * @param Service the name of the Online Service to force default online service to return
	 */
	ONLINESERVICESINTERFACE_API void SetDefaultServiceOverride(EOnlineServices DefaultService);

	/**
	 * Removes an override for the target service.
	 */
	ONLINESERVICESINTERFACE_API void ClearDefaultServiceOverride();
#endif //WITH_DEV_AUTOMATION_TESTS
private:
	struct FFactoryAndPriority
	{
		FFactoryAndPriority(TUniquePtr<IOnlineServicesFactory>&& InFactory, int32 InPriority)
			: Factory(MoveTemp(InFactory))
			, Priority(InPriority)
		{
		}

		TUniquePtr<IOnlineServicesFactory> Factory;
		int32 Priority;
	};

	TMap<EOnlineServices, FFactoryAndPriority> ServicesFactories;
	TMap<EOnlineServices, TMap<FName, TSharedRef<IOnlineServices>>> NamedServiceInstances;
	EOnlineServices DefaultServiceOverride = EOnlineServices::Default;

	FOnlineServicesRegistry() {}
	~FOnlineServicesRegistry();
	friend FLazySingleton;
};

/* UE::Online */ }
