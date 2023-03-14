// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"

#include "Misc/TextureShareCoreStrings.h"
#include "Misc/TextureShareStrings.h"
#include "Misc/TextureShareDisplayClusterStrings.h"

/**
 * Resource sync steps helpers (used in resource desc constructors)
 */
namespace TextureShareResourceSyncStepHelper
{
	inline bool IsEqual(const wchar_t* InResourceName1, const wchar_t* InResourceName2)
	{
		return FPlatformString::Strcmp(InResourceName1, InResourceName2) == 0;
	}

	// Return sync step for resource by name and op type
	// related to sync logic in TextureShareDisplayClusterProxyObject and TextureShareWorldSubsystemProxyObject
	static ETextureShareSyncStep GetDisplayClusterSyncStep(const wchar_t* InResourceName, const ETextureShareTextureOp InOperationType)
	{
		// Display cluster sync steps for resources:
		if (IsEqual(InResourceName, TextureShareDisplayClusterStrings::Output::Backbuffer)
			|| IsEqual(InResourceName, TextureShareDisplayClusterStrings::Output::BackbufferTemp))
		{
			return (InOperationType == ETextureShareTextureOp::Read) ? ETextureShareSyncStep::FrameProxyPostRenderBegin : ETextureShareSyncStep::FrameProxyPostRenderEnd;
		}
		else if (IsEqual(InResourceName, TextureShareDisplayClusterStrings::Viewport::Input) || IsEqual(InResourceName, TextureShareDisplayClusterStrings::Viewport::Mips))
		{
			return (InOperationType == ETextureShareTextureOp::Read) ? ETextureShareSyncStep::FrameProxyRenderBegin : ETextureShareSyncStep::FrameProxyRenderEnd;
		}
		else if (IsEqual(InResourceName, TextureShareDisplayClusterStrings::Viewport::Warped))
		{
			return (InOperationType == ETextureShareTextureOp::Read) ? ETextureShareSyncStep::FrameProxyPostWarpBegin : ETextureShareSyncStep::FrameProxyPostWarpEnd;
		}
		else if (IsEqual(InResourceName, TextureShareStrings::SceneTextures::Backbuffer))
		{
			return (InOperationType == ETextureShareTextureOp::Read) ? ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentBegin : ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentEnd;
		}

		// Default sync steps
		return (InOperationType == ETextureShareTextureOp::Read) ? ETextureShareSyncStep::FrameProxyPreRenderBegin : ETextureShareSyncStep::FrameProxyPreRenderEnd;
	}

	static ETextureShareSyncStep GeSyncStep(const wchar_t* InResourceName, const ETextureShareTextureOp InOperationType)
	{
		// Display cluster sync steps for resources:
		if(IsEqual(InResourceName, TextureShareStrings::SceneTextures::SceneColor)
		|| IsEqual(InResourceName, TextureShareStrings::SceneTextures::SceneDepth)
		|| IsEqual(InResourceName, TextureShareStrings::SceneTextures::SmallDepthZ)
		|| IsEqual(InResourceName, TextureShareStrings::SceneTextures::GBufferA)
		|| IsEqual(InResourceName, TextureShareStrings::SceneTextures::GBufferB)
		|| IsEqual(InResourceName, TextureShareStrings::SceneTextures::GBufferC)
		|| IsEqual(InResourceName, TextureShareStrings::SceneTextures::GBufferD)
		|| IsEqual(InResourceName, TextureShareStrings::SceneTextures::GBufferE)
		|| IsEqual(InResourceName, TextureShareStrings::SceneTextures::GBufferF))
		{
			return ETextureShareSyncStep::FrameSceneFinalColorBegin;
		}
		else if (IsEqual(InResourceName, TextureShareStrings::SceneTextures::FinalColor))
		{
			return (InOperationType == ETextureShareTextureOp::Read) ? ETextureShareSyncStep::FrameSceneFinalColorBegin : ETextureShareSyncStep::FrameSceneFinalColorEnd;
		}
		else if (IsEqual(InResourceName, TextureShareStrings::SceneTextures::Backbuffer))
		{
			return (InOperationType == ETextureShareTextureOp::Read) ? ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentBegin : ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentEnd;
		}

		// Default sync steps
		return (InOperationType == ETextureShareTextureOp::Read) ? ETextureShareSyncStep::FrameProxyPreRenderBegin : ETextureShareSyncStep::FrameProxyPreRenderEnd;
	}
}
