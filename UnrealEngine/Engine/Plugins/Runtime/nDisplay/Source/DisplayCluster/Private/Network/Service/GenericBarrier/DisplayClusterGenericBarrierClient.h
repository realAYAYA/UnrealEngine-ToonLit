// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Network/DisplayClusterClient.h"
#include "Network/Packet/DisplayClusterPacketInternal.h"
#include "Network/Protocol/IDisplayClusterProtocolGenericBarrier.h"
#include "Cluster/IDisplayClusterGenericBarriersClient.h"


/**
 * Generic barriers TCP client
 */
class FDisplayClusterGenericBarrierClient
	: public FDisplayClusterClient<FDisplayClusterPacketInternal>
	, public IDisplayClusterProtocolGenericBarrier
	, public IDisplayClusterGenericBarriersClient
{
public:
	FDisplayClusterGenericBarrierClient();
	FDisplayClusterGenericBarrierClient(const FString& InName);

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClient
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Connect(const FString& Address, const uint16 Port, const uint32 ConnectRetriesAmount, const uint32 ConnectRetryDelay) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterGenericBarriersClient
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Connect() override;
	virtual void Disconnect() override;
	virtual bool IsConnected() const override;

	virtual FString GetName() const override;

	virtual bool CreateBarrier(const FString& BarrierId, const TArray<FString>& UniqueThreadMarkers, uint32 Timeout) override;
	virtual bool WaitUntilBarrierIsCreated(const FString& BarrierId) override;
	virtual bool IsBarrierAvailable(const FString& BarrierId) override;
	virtual FOnGenericBarrierSynchronizationDelegate* GetBarrierSyncDelegate(const FString& BarrierId) override;
	virtual bool ReleaseBarrier(const FString& BarrierId) override;
	virtual bool Synchronize(const FString& BarrierId, const FString& UniqueThreadMarker) override;
	virtual bool Synchronize(const FString& BarrierId, const FString& UniqueThreadMarker, const TArray<uint8>& RequestData, TArray<uint8>& OutResponseData) override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolGenericBarrier
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult CreateBarrier(const FString& BarrierId, const TArray<FString>& UniqueThreadMarkers, uint32 Timeout, EBarrierControlResult& Result) override;
	virtual EDisplayClusterCommResult WaitUntilBarrierIsCreated(const FString& BarrierId, EBarrierControlResult& Result) override;
	virtual EDisplayClusterCommResult IsBarrierAvailable(const FString& BarrierId, EBarrierControlResult& Result) override;
	virtual EDisplayClusterCommResult ReleaseBarrier(const FString& BarrierId, EBarrierControlResult& Result) override;
	virtual EDisplayClusterCommResult SyncOnBarrier(const FString& BarrierId, const FString& UniqueThreadMarker, EBarrierControlResult& Result) override;
	virtual EDisplayClusterCommResult SyncOnBarrierWithData(const FString& BarrierId, const FString& UniqueThreadMarker, const TArray<uint8>& RequestData, TArray<uint8>& OutResponseData, EBarrierControlResult& Result) override;

private:
	/** Setup/release sync delegate for a specific barrier */
	bool ConfigureBarrierSyncDelegate(const FString& BarrierId, bool bSetup);

	/** Callback on barrier sync phase end */
	void OnPreBarrierSyncEnd(const FString& BarrierId, const TMap<FString, TArray<uint8>>& RequestData, TMap<FString, TArray<uint8>>& ResponseData);

private:
	// Holds synchronization delegates for the barriers operated by this client
	TMap<FString, FOnGenericBarrierSynchronizationDelegate> BarrierSyncDelegates;
};
