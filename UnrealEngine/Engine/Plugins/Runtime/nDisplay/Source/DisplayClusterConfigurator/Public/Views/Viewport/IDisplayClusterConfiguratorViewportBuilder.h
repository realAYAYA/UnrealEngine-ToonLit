// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/IDisplayClusterConfiguratorBuilder.h"


class IDisplayClusterConfiguratorViewportBuilder
	: public IDisplayClusterConfiguratorBuilder
{
public:
	virtual ~IDisplayClusterConfiguratorViewportBuilder() = default;

public:
	virtual void BuildViewport() = 0;

	virtual void ClearViewportSelection() = 0;
};
