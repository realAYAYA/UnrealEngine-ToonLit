// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IDisplayClusterConfiguratorBuilder
	: public TSharedFromThis<IDisplayClusterConfiguratorBuilder>
{
public:
	virtual ~IDisplayClusterConfiguratorBuilder() = default;
};
