// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterConfigurationVersion.h"

class UDisplayClusterConfigurationData;


/**
 * Display Cluster configuration manager
 */
class FDisplayClusterConfigurationMgr
{
protected:
	FDisplayClusterConfigurationMgr() = default;
	~FDisplayClusterConfigurationMgr() = default;

public:
	// Singletone getter
	static FDisplayClusterConfigurationMgr& Get();

public:
	EDisplayClusterConfigurationVersion GetConfigVersion(const FString& FilePath);
	UDisplayClusterConfigurationData* LoadConfig(const FString& FilePath, UObject* Owner = nullptr);
	bool SaveConfig(const UDisplayClusterConfigurationData* Config, const FString& FilePath);
	bool ConfigAsString(const UDisplayClusterConfigurationData* Config, FString& OutString);
	UDisplayClusterConfigurationData* CreateDefaultStandaloneConfigData();
};
