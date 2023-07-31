// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Network/Service/DisplayClusterService.h"
#include "Network/Session/IDisplayClusterSessionPacketHandler.h"
#include "Network/Protocol/IDisplayClusterProtocolRenderSync.h"
#include "Network/Packet/DisplayClusterPacketInternal.h"

class IDisplayClusterBarrier;
struct FIPv4Endpoint;


/**
 * Rendering synchronization TCP server
 */
class FDisplayClusterRenderSyncService
	: public    FDisplayClusterService
	, public    IDisplayClusterSessionPacketHandler<FDisplayClusterPacketInternal, true>
	, protected IDisplayClusterProtocolRenderSync
{
public:
	FDisplayClusterRenderSyncService();
	virtual ~FDisplayClusterRenderSyncService();

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

	// Callback on barrier timeout
	void ProcessBarrierTimeout(const FString& BarrierName, const TArray<FString>& NodesTimedOut);

protected:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterSessionPacketHandler
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual TSharedPtr<FDisplayClusterPacketInternal> ProcessPacket(const TSharedPtr<FDisplayClusterPacketInternal>& Request, const FDisplayClusterSessionInfo& SessionInfo) override;

private:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProtocolRenderSync
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual EDisplayClusterCommResult WaitForSwapSync() override;

private:
	// Swap sync barrier
	TUniquePtr<IDisplayClusterBarrier> BarrierSwap;

	// Auxiliary container that keeps all the barriers
	TMap<FString, TUniquePtr<IDisplayClusterBarrier>*> ServiceBarriers;
};
