// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/PostProcess/IDisplayClusterPostProcessFactory.h"

/**
 * Postprocess factory for TextureShare over nDisplay
 */
class FTextureSharePostprocessFactory
	: public IDisplayClusterPostProcessFactory
{
public:
	// Constructor for postprocess
	virtual TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> Create(const FString& PostProcessId, const struct FDisplayClusterConfigurationPostprocess* InConfigurationPostProcess) override;
};
