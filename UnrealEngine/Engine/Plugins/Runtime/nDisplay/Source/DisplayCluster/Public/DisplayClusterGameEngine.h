// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameEngine.h"

#include "DisplayClusterEnums.h"
#include "DisplayClusterConfigurationTypes.h"

#include "Cluster/DisplayClusterClusterEvent.h"
#include "DisplayClusterGameEngine.generated.h"


class IPDisplayClusterClusterManager;
class IDisplayClusterClusterNodeController;
class IDisplayClusterClusterSyncObject;
class UDisplayClusterConfigurationData;

DECLARE_DELEGATE_RetVal_ThreeParams(EBrowseReturnVal::Type, FOnBrowseLoadMap, FWorldContext& WorldContext, FURL URL, FString& Error);
DECLARE_DELEGATE_TwoParams(FOnPengineLevelUpdate, FWorldContext& Context, float DeltaSeconds);

/**
 * Extended game engine
 */
UCLASS()
class DISPLAYCLUSTER_API UDisplayClusterGameEngine
	: public UGameEngine
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject = nullptr;

public:
	virtual void Init(class IEngineLoop* InEngineLoop) override;
	virtual void PreExit() override;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	virtual bool LoadMap(FWorldContext& WorldContext, FURL URL, class UPendingNetGame* Pending, FString& Error) override;

public:
	EDisplayClusterOperationMode GetOperationMode() const
	{
		return OperationMode;
	}

protected:
	virtual void UpdateTimeAndHandleMaxTickRate() override;

protected:
	virtual bool InitializeInternals();
	EDisplayClusterOperationMode DetectOperationMode() const;
	bool GetResolvedNodeId(const UDisplayClusterConfigurationData* ConfigData, FString& NodeId) const;
	bool ValidateConfigFile(const FString& FilePath);
	void PreExitImpl();

private:
	bool OutOfSync() const;
	void ReceivedSync(const FString &Level, const FString &NodeId);

	void CheckGameStartBarrier();
	bool CanTick() const;

	bool BarrierAvoidanceOn() const;

	void GameSyncChange(const FDisplayClusterClusterEventJson& InEvent);
	
	/** Implements custom loading logic to support listening server
	 */
	EBrowseReturnVal::Type BrowseLoadMap(FWorldContext& WorldContext, FURL URL, FString& Error);

	/** Custom logic to update PendingLevel in order to support listening server
	 */
	void PendingLevelUpdate(FWorldContext& Context, float DeltaSeconds);
private:
	IPDisplayClusterClusterManager* ClusterMgr = nullptr;

	FDisplayClusterConfigurationDiagnostics Diagnostics;

	EDisplayClusterOperationMode OperationMode = EDisplayClusterOperationMode::Disabled;
	EDisplayClusterRunningMode   RunningMode = EDisplayClusterRunningMode::Startup;

	TMap<FString, TSet<FString>> SyncMap;
};
