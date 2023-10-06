// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/EasyBlend/DisplayClusterProjectionEasyBlendPolicyBase.h"
#include "Policy/EasyBlend/Windows/DX11/DisplayClusterProjectionEasyBlendViewAdapterDX11.h"

#include "DisplayClusterProjectionLog.h"


/**
 * EasyBlend projection policy (DX11 implementation)
 */
class FDisplayClusterProjectionEasyBlendPolicyDX11
	: public FDisplayClusterProjectionEasyBlendPolicyBase
{
public:
	FDisplayClusterProjectionEasyBlendPolicyDX11(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy)
		: FDisplayClusterProjectionEasyBlendPolicyBase(ProjectionPolicyId, InConfigurationProjectionPolicy)
	{ }

	virtual bool IsEasyBlendRenderingEnabled() override
	{
		return FDisplayClusterProjectionEasyBlendViewAdapterDX11::IsEasyBlendRenderingEnabled();
	}

protected:
	virtual TUniquePtr<FDisplayClusterProjectionEasyBlendViewAdapterBase> CreateViewAdapter(const FDisplayClusterProjectionEasyBlendViewAdapterBase::FInitParams& InitParams) override
	{
		UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("Instantiating EasyBlend DX11 viewport adapter..."));
		return MakeUnique<FDisplayClusterProjectionEasyBlendViewAdapterDX11>(InitParams);
	}
};
