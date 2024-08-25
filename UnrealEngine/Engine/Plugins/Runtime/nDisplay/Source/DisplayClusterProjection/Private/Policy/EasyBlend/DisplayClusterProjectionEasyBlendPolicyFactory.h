// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"


/**
 * Implements projection policy factory for the 'EasyBlend' policy
 */
class FDisplayClusterProjectionEasyBlendPolicyFactory
	: public IDisplayClusterProjectionPolicyFactory
{
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicyFactory
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> Create(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy) override;

private:
	TSharedPtr<class FDisplayClusterProjectionEasyBlendLibraryDX11, ESPMode::ThreadSafe> EasyBlendLibraryDX11;
	TSharedPtr<class FDisplayClusterProjectionEasyBlendLibraryDX12, ESPMode::ThreadSafe> EasyBlendLibraryDX12;
};
