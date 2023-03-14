// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/IDisplayClusterConfiguratorItem.h"

class IDisplayClusterConfiguratorOutputMappingSlot;

/**
 * The Interface holsd object and setting from Details View
 */
class IDisplayClusterConfiguratorOutputMappingItem
	: public IDisplayClusterConfiguratorItem
{
public:
	virtual ~IDisplayClusterConfiguratorOutputMappingItem() = default;

public:
	virtual const FString& GetNodeName() const = 0;
};
