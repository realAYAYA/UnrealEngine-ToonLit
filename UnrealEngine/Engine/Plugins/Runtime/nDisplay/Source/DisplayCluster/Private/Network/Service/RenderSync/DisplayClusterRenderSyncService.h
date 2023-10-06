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
	virtual EDisplayClusterCommResult SyncOnBarrier() override;

private:
	// Initializes barriers based on available nodes and their sync policies
	void InitializeBarriers();

	// Activates all service barriers
	void ActivateAllBarriers();
	// Deactivates all service barriers
	void DeactivateAllBarriers();

	// Unsubscribe from all events of all internal barriers
	void UnsubscribeFromAllBarrierEvents();

	// Returns barrier of a node's sync group
	IDisplayClusterBarrier* GetBarrierForNode(const FString& NodeId) const;

	// Unregister cluster node from a barrier
	void UnregisterClusterNode(const FString& NodeId);

private:
	// We can now use different sync policies within the same cluster. Since each policy may
	// have its own synchronization logic, mainly barriers utilization, we need to have
	// individual sync barriers for every sync group. A sync group is a list of nodes that
	// use the same sync policy.
	TMap<FString, TUniquePtr<IDisplayClusterBarrier>> PolicyToBarrierMap;

	// Node ID to sync policy ID (or sync group) mapping
	TMap<FString, FString> NodeToPolicyMap;
};
