// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterConfiguration.h"

class UDisplayClusterConfigurationData;


/**
 * Display Cluster configuration module
 */
class FDisplayClusterConfigurationModule :
	public IDisplayClusterConfiguration
{
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IModuleInterface
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void StartupModule() override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterConfiguration
	//////////////////////////////////////////////////////////////////////////////////////////////

	virtual void SetIsSnapshotTransacting(bool bIsSnapshot) override;
	virtual bool IsTransactingSnapshot() const override;
	virtual EDisplayClusterConfigurationVersion GetConfigVersion(const FString& FilePath) override;
	virtual UDisplayClusterConfigurationData* LoadConfig(const FString& FilePath, UObject* Owner = nullptr) override;
	virtual bool SaveConfig(const UDisplayClusterConfigurationData* Config, const FString& FilePath) override;
	virtual bool ConfigAsString(const UDisplayClusterConfigurationData* Config, FString& OutString) const override;

private:
	/** Snapshot state */
	bool bIsSnapshot = false;
};
