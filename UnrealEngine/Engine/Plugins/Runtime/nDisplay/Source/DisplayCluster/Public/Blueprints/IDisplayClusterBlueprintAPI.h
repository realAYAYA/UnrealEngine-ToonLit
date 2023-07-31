// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "DisplayClusterEnums.h"
#include "Cluster/DisplayClusterClusterEvent.h"

#include "Engine/Scene.h"
#include "SceneViewExtension.h"

// Forward declarations for the following classes leads to a build error
#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterOriginComponent.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "DisplayClusterConfigurationTypes.h"

#include "IDisplayClusterBlueprintAPI.generated.h"


class IDisplayClusterClusterEventListener;
class UCineCameraComponent;
struct FPostProcessSettings;


UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class DISPLAYCLUSTER_API UDisplayClusterBlueprintAPI : public UInterface
{
	GENERATED_BODY()
};

/**
 * Blueprint API interface
 */
class DISPLAYCLUSTER_API IDisplayClusterBlueprintAPI
{
	GENERATED_BODY()

public:
	/* Returns true if the module has been initialized. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is module initialized"), Category = "NDisplay")
	virtual bool IsModuleInitialized() const = 0;

	/** Returns current operation mode. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get operation mode"), Category = "NDisplay")
	virtual EDisplayClusterOperationMode GetOperationMode() const = 0;

public:
	/** Returns DisplayCluster root actor. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get root actor"), Category = "NDisplay|Game")
	virtual ADisplayClusterRootActor* GetRootActor() const = 0;

	// Runtime local node API

	/** Returns Id of the current node in a cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get node ID"), Category = "NDisplay|Cluster")
	virtual FString GetNodeId() const = 0;

	/** Returns List of the active nodes in the runtime cluster node in a cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get cluster node IDs"), Category = "NDisplay|Cluster")
	virtual void GetActiveNodeIds(TArray<FString>& OutNodeIds) const = 0;

	/** Returns amount of active nodes in a cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get nodes amount"), Category = "NDisplay|Cluster")
	virtual int32 GetActiveNodesAmount() const = 0;

	/** Returns true if current node is a primary node in a cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is a primary node"), Category = "NDisplay|Cluster")
	virtual bool IsPrimary() const = 0;

	/** Returns true if current node is a secondary node in a cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is a secondary node"), Category = "NDisplay|Cluster")
	virtual bool IsSecondary() const = 0;

	/** Returns true if current node is a backup node in a cluster. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Is a backup node"), Category = "NDisplay|Cluster")
	virtual bool IsBackup() const = 0;

	/** Returns the role of the current cluster node. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get cluster role"), Category = "NDisplay|Cluster")
	virtual EDisplayClusterNodeRole GetClusterRole() const = 0;

	// cluster events api

	/** Adds cluster event listener. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Add cluster event listener"), Category = "NDisplay|Cluster")
	virtual void AddClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener) = 0;

	/** Removes cluster event listener. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Remove cluster event listener"), Category = "NDisplay|Cluster")
	virtual void RemoveClusterEventListener(TScriptInterface<IDisplayClusterClusterEventListener> Listener) = 0;

	/** Emits JSON cluster event. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Emit JSON cluster event"), Category = "NDisplay|Cluster")
	virtual void EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly) = 0;

	/** Emits binary cluster event. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Emit binary cluster event"), Category = "NDisplay|Cluster")
	virtual void EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly) = 0;

	/** Sends JSON cluster event to a specific target (outside of the cluster). */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Send JSON event to a specific host"), Category = "NDisplay|Cluster")
	virtual void SendClusterEventJsonTo(const FString& Address, const int32 Port, const FDisplayClusterClusterEventJson& Event, bool bPrimaryOnly) = 0;

	/** Sends binary cluster event to a specific target (outside of the cluster). */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Send binary event to a specific host"), Category = "NDisplay|Cluster")
	virtual void SendClusterEventBinaryTo(const FString& Address, const int32 Port, const FDisplayClusterClusterEventBinary& Event, bool bPrimaryOnly) = 0;
};
