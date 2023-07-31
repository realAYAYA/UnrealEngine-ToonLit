// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"
#include "Policy/DisplayClusterProjectionPolicyBase.h"


/**
 * Implements projection policy factory for the 'VIOSO' policy
 */
class FDisplayClusterProjectionVIOSOPolicyFactory
	: public IDisplayClusterProjectionPolicyFactory
{
public:
	FDisplayClusterProjectionVIOSOPolicyFactory() = default;
	virtual ~FDisplayClusterProjectionVIOSOPolicyFactory() = default;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicyFactory
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> Create(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy) override;
};
