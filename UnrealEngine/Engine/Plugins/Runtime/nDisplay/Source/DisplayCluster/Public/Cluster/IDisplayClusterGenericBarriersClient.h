// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * Barreir synchonrization callback data
 */
struct FGenericBarrierSynchronizationDelegateData
{
	/** Barrier ID */
	const FString BarrierId;

	/** Binary data provided on sync request (node Id - to - data mapping) */
	const TMap<FString, TArray<uint8>>& RequestData;

	/** Binary data to respond (node Id - to - data mapping) */
	TMap<FString, TArray<uint8>>& ResponseData;
};


/**
 * Generic barriers client interface
 */
class IDisplayClusterGenericBarriersClient
{
public:
	virtual ~IDisplayClusterGenericBarriersClient() = default;

// Networking API
public:
	/** Connects to a server */
	virtual bool Connect() = 0;

	/** Terminates current connection */
	virtual void Disconnect() = 0;

	/** Returns connection status */
	virtual bool IsConnected() const = 0;

	/** Returns client name */
	virtual FString GetName() const = 0;

// Barrier API
public:
	/** Creates new barrier */
	virtual bool CreateBarrier(const FString& BarrierId, const TArray<FString>& UniqueThreadMarkers, uint32 Timeout) = 0;

	/** Wait until a barrier with specific ID is created and ready to go */
	virtual bool WaitUntilBarrierIsCreated(const FString& BarrierId) = 0;

	/** Checks if a specific barrier exists */
	virtual bool IsBarrierAvailable(const FString& BarrierId) = 0;

	/** Returns a synchronization delegate to a specific barrier or nullptr if not available */
	DECLARE_DELEGATE_OneParam(FOnGenericBarrierSynchronizationDelegate, FGenericBarrierSynchronizationDelegateData&);
	virtual FOnGenericBarrierSynchronizationDelegate* GetBarrierSyncDelegate(const FString& BarrierId) = 0;

	/** Releases specific barrier */
	virtual bool ReleaseBarrier(const FString& BarrierId) = 0;

	/** Synchronize calling thread on a specific barrier */
	virtual bool Synchronize(const FString& BarrierId, const FString& UniqueThreadMarker) = 0;

	/** Synchronize calling thread on a specific barrier with custom data */
	virtual bool Synchronize(const FString& BarrierId, const FString& UniqueThreadMarker, const TArray<uint8>& RequestData, TArray<uint8>& OutResponseData) = 0;
};

/**
 * Custom deleter to avoid calling the virtual Disconnect function in the destructor.
 * This is necessary since the destructor of the base class is calling it but expecting
 * the derived class implementation to be called. This also avoids the PVS warning:
 * V1053: Calling the 'Disconnect' virtual function in the destructor may lead to unexpected result at runtime.
 */
template<typename ClientType>
struct FDisplayClusterGenericClientDeleter
{
	void operator()(ClientType* Client)
	{
		if (Client && Client->IsConnected())
		{
			Client->Disconnect();
		}

		delete Client;
	}
};

// Custom deleter for IDisplayClusterGenericBarriersClient.
using FDisplayClusterGenericBarriersClientDeleter = FDisplayClusterGenericClientDeleter<IDisplayClusterGenericBarriersClient>;
