// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Network/Service/DisplayClusterService.h"
#include "Network/Session/IDisplayClusterSessionPacketHandler.h"
#include "Network/Protocol/IDisplayClusterProtocolClusterSync.h"
#include "Network/Packet/DisplayClusterPacketInternal.h"

class IDisplayClusterBarrier;


/**
 * Cluster synchronization TCP server
 */
class FDisplayClusterClusterSyncService
	: public    FDisplayClusterService
	, public    IDisplayClusterSessionPacketHandler<FDisplayClusterPacketInternal, true>
	, protected IDisplayClusterProtocolClusterSync
{
public:
	FDisplayClusterClusterSyncService();
	virtual ~FDisplayClusterClusterSyncService();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterServer
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool Start(const FString& Address, const uint16 Port) override;
	virtual bool Start(TSharedPtr<FDisplayClusterTcpListener>& ExternalListener) override;
	virtual void Shutdown() override final;
	virtual FString GetProtocolName() const override;
	virtual void KillSession(const FString& NodeId) override;

protected:
	// Creates session instance for this service
	virtual TSharedPtr<IDisplayClusterSession> CreateSession(FDisplayClusterSessionInfo& SessionInfo) override;

	// Callback when a session is closed
	void ProcessSessionClosed(const FDisplayClusterSessionInfo& SessionInfo);

	// Callbck on barrier timeout
	void ProcessBarrierTimeout(const FString& BarrierName, const TArray<FString>& NodesTimedOut);

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionPacketHandler
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual TSharedPtr<FDisplayClusterPacketInternal> ProcessPacket(const TSharedPtr<FDisplayClusterPacketInternal>& Request, const FDisplayClusterSessionInfo& SessionInfo) override;

private:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolClusterSync
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult WaitForGameStart() override;
	virtual EDisplayClusterCommResult WaitForFrameStart() override;
	virtual EDisplayClusterCommResult WaitForFrameEnd() override;
	virtual EDisplayClusterCommResult GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime) override;
	virtual EDisplayClusterCommResult GetObjectsData(EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData)  override;
	virtual EDisplayClusterCommResult GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson, ESPMode::ThreadSafe>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary, ESPMode::ThreadSafe>>& OutBinaryEvents) override;
	virtual EDisplayClusterCommResult GetNativeInputData(TMap<FString, FString>& OutNativeInputData) override;

private:
	// Game start sync barrier
	TUniquePtr<IDisplayClusterBarrier> BarrierGameStart;
	// Frame start barrier
	TUniquePtr<IDisplayClusterBarrier> BarrierFrameStart;
	// Frame end barrier
	TUniquePtr<IDisplayClusterBarrier> BarrierFrameEnd;

	// Auxiliary container that keeps all the barriers
	TMap<FString, TUniquePtr<IDisplayClusterBarrier>*> ServiceBarriers;
};
