// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterBlueprintAPIImpl.h"

#include "IDisplayCluster.h"

#include "Cluster/DisplayClusterClusterEvent.h"
#include "Cluster/IDisplayClusterClusterManager.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Game/IDisplayClusterGameManager.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "Render/Device/IDisplayClusterRenderDevice.h"
#include "DisplayClusterSceneViewExtensions.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterTypesConverter.h"
#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterEnums.h"

#include "Math/IntRect.h"

#include "CineCameraComponent.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"

// Cluster runtime API

bool UDisplayClusterBlueprintAPIImpl::IsModuleInitialized() const
{
	const bool bInitialized = IDisplayCluster::Get().IsModuleInitialized();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("IsModuleInitialized - %s"), *DisplayClusterTypesConverter::template ToString(bInitialized));
	return bInitialized;
}

EDisplayClusterOperationMode UDisplayClusterBlueprintAPIImpl::GetOperationMode() const
{
	const EDisplayClusterOperationMode OpMode = IDisplayCluster::Get().GetOperationMode();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("Operation mode - %s"), *DisplayClusterTypesConverter::template ToString(OpMode));
	return OpMode;
}

ADisplayClusterRootActor* UDisplayClusterBlueprintAPIImpl::GetRootActor() const
{
	ADisplayClusterRootActor* const RootActor = IDisplayCluster::Get().GetGameMgr()->GetRootActor();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("GetRootActor - %s"), RootActor ? *RootActor->GetHumanReadableName() : DisplayClusterStrings::log::NotFound);
	return RootActor;
}

// Local node runtime API

FString UDisplayClusterBlueprintAPIImpl::GetNodeId() const
{
	const FString NodeId = IDisplayCluster::Get().GetClusterMgr()->GetNodeId();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("GetNodeId - NodeId=%s"), *NodeId);
	return NodeId;
}

void UDisplayClusterBlueprintAPIImpl::GetActiveNodeIds(TArray<FString>& OutNodeIds) const
{
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("GetNodeIds"));
	IDisplayCluster::Get().GetClusterMgr()->GetNodeIds(OutNodeIds);
}

int32 UDisplayClusterBlueprintAPIImpl::GetActiveNodesAmount() const
{
	const int32 NodesAmount = IDisplayCluster::Get().GetClusterMgr()->GetNodesAmount();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("GetNodesAmount - %d"), NodesAmount);
	return NodesAmount;
}

bool UDisplayClusterBlueprintAPIImpl::IsPrimary() const
{
	const bool bIsPrimary = IDisplayCluster::Get().GetClusterMgr()->IsPrimary();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("IsPrimary - %s"), *DisplayClusterTypesConverter::template ToString(bIsPrimary));
	return bIsPrimary;
}

bool UDisplayClusterBlueprintAPIImpl::IsSecondary() const
{
	const bool bIsSecondary = IDisplayCluster::Get().GetClusterMgr()->IsSecondary();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("IsSecondary - %s"), *DisplayClusterTypesConverter::template ToString(bIsSecondary));
	return bIsSecondary;
}

bool UDisplayClusterBlueprintAPIImpl::IsBackup() const
{
	const bool bIsBackup = IDisplayCluster::Get().GetClusterMgr()->IsBackup();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("IsBackup - %s"), *DisplayClusterTypesConverter::template ToString(bIsBackup));
	return bIsBackup;
}

EDisplayClusterNodeRole UDisplayClusterBlueprintAPIImpl::GetClusterRole() const
{
	const EDisplayClusterNodeRole ClusterRole = IDisplayCluster::Get().GetClusterMgr()->GetClusterRole();
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("GetClusterRole - %d"), static_cast<int32>(ClusterRole));
	return ClusterRole;
}

// Cluster events API

void UDisplayClusterBlueprintAPIImpl::AddClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("AddClusterEventListener - adding cluster event listener..."));
	IDisplayCluster::Get().GetClusterMgr()->AddClusterEventListener(Listener);
}

void UDisplayClusterBlueprintAPIImpl::RemoveClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener)
{
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("RemoveClusterEventListener - removing cluster event listener..."));
	IDisplayCluster::Get().GetClusterMgr()->RemoveClusterEventListener(Listener);
}

void UDisplayClusterBlueprintAPIImpl::EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly)
{
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("EmitClusterEventJson - emitting cluster event: bPrimaryOnly='%s' Category='%s' Type='%s' Name='%s'"),
		*DisplayClusterTypesConverter::template ToString(bPrimaryOnly), *Event.Category, *Event.Type, *Event.Name);

	IDisplayCluster::Get().GetClusterMgr()->EmitClusterEventJson(Event, bPrimaryOnly);
}

void UDisplayClusterBlueprintAPIImpl::EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly)
{
	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("EmitClusterEventBinary - emitting cluster event: bPrimaryOnly='%s' EventId='%d'"),
		*DisplayClusterTypesConverter::template ToString(bPrimaryOnly), Event.EventId);

	IDisplayCluster::Get().GetClusterMgr()->EmitClusterEventBinary(Event, bPrimaryOnly);
}

void UDisplayClusterBlueprintAPIImpl::SendClusterEventJsonTo(const FString& Address, const int32 Port, const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly)
{
	checkSlow(Port > 0 && Port <= UINT16_MAX);

	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("SendClusterEventJsonTo - sending json event to %s:%d, Category='%s' Type='%s' Name='%s'"), *Address, Port, *Event.Category, *Event.Type, *Event.Name);
	IDisplayCluster::Get().GetClusterMgr()->SendClusterEventTo(Address, static_cast<uint16>(Port), Event, bPrimaryOnly);
}

void UDisplayClusterBlueprintAPIImpl::SendClusterEventBinaryTo(const FString& Address, const int32 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly)
{
	checkSlow(Port > 0 && Port <= UINT16_MAX);

	UE_LOG(LogDisplayClusterBlueprint, Verbose, TEXT("SendClusterEventBinaryTo - sending binary event to %s:%d, EventId='%d'"), *Address, Port, Event.EventId);
	IDisplayCluster::Get().GetClusterMgr()->SendClusterEventTo(Address, static_cast<uint16>(Port), Event, bPrimaryOnly);
}
