// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Device/IDisplayClusterRenderDeviceFactory.h"


/**
 * Factory for internal rendering devices
 */
class FDisplayClusterRenderDeviceFactoryInternal
	: public IDisplayClusterRenderDeviceFactory
{
public:
	FDisplayClusterRenderDeviceFactoryInternal() = default;
	virtual ~FDisplayClusterRenderDeviceFactoryInternal() = default;

public:
	virtual TSharedPtr<IDisplayClusterRenderDevice, ESPMode::ThreadSafe> Create(const FString& InDeviceType) override;
};
