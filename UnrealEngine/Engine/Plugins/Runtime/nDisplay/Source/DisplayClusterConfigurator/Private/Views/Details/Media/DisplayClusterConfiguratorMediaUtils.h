// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IDisplayClusterModularFeatureMediaInitializer;

/**
 * Media customization utilities
 */
class FDisplayClusterConfiguratorMediaUtils
{
protected:
	FDisplayClusterConfiguratorMediaUtils();

public:

	/** Singleton access */
	static FDisplayClusterConfiguratorMediaUtils& Get();

public:

	/** Returns media initializers */
	const TArray<IDisplayClusterModularFeatureMediaInitializer*>& GetMediaInitializers() const
	{
		return MediaInitializers;
	}

	/** Tiled media auto-configuration event */
	DECLARE_EVENT_OneParam(FDisplayClusterConfiguratorMediaUtils, FDisplayClusterConfiguratorMediaAutoConfigurationRequestedEvent, UObject*);
	FDisplayClusterConfiguratorMediaAutoConfigurationRequestedEvent& OnMediaAutoConfiguration()
	{
		return MediaAutoConfigurationEvent;
	}

private:

	/** Media initializers (modular features) available. */
	TArray<IDisplayClusterModularFeatureMediaInitializer*> MediaInitializers;

	/** Tiled media auto-configuration event */
	FDisplayClusterConfiguratorMediaAutoConfigurationRequestedEvent MediaAutoConfigurationEvent;
};
