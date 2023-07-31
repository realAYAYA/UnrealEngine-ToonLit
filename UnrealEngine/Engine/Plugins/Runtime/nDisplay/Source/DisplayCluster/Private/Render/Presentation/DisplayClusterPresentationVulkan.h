// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Presentation/DisplayClusterPresentationBase.h"


/**
 * Helper class to encapsulate Vulkan frame presentation
 */
class FDisplayClusterPresentationVulkan : public FDisplayClusterPresentationBase
{
public:
	FDisplayClusterPresentationVulkan(FViewport* const InViewport, TSharedPtr<IDisplayClusterRenderSyncPolicy>& InSyncPolicy)
		: FDisplayClusterPresentationBase(InViewport, InSyncPolicy)
	{ }

	virtual ~FDisplayClusterPresentationVulkan() = default;
};
