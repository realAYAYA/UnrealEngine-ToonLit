// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Cluster/IDisplayClusterClusterSyncObject.h"
#include "DisplayClusterSceneComponentSync.generated.h"

class IPDisplayClusterGameManager;
class IPDisplayClusterClusterManager;


/**
 * Abstract synchronization component
 */
UCLASS(Abstract)
class DISPLAYCLUSTER_API UDisplayClusterSceneComponentSync
	: public USceneComponent
	, public IDisplayClusterClusterSyncObject
{
	GENERATED_BODY()

public:
	UDisplayClusterSceneComponentSync(const FObjectInitializer& ObjectInitializer);
	
	virtual ~UDisplayClusterSceneComponentSync()
	{ }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterClusterSyncObject
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool IsActive() const override;
	
	virtual FString GetSyncId() const override
	{
		return SyncId;
	}
	
	virtual bool IsDirty() const override
	{
		return true;
	}

	virtual void ClearDirty() override
	{ }

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterStringSerializable
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual FString SerializeToString() const override;
	virtual bool    DeserializeFromString(const FString& data) override;

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual FString GenerateSyncId();

	virtual FTransform GetSyncTransform() const
	{ return FTransform(); }

	virtual void SetSyncTransform(const FTransform& t)
	{ }

protected:
	IPDisplayClusterGameManager*    GameMgr = nullptr;
	IPDisplayClusterClusterManager* ClusterMgr = nullptr;

protected:
	// Caching state
	FVector  LastSyncLoc;
	FRotator LastSyncRot;
	FVector  LastSyncScale;

private:
	FString SyncId;
};
