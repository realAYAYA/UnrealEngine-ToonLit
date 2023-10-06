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
		if (IsEqual(InResourceName, UE::TextureShare::DisplayClusterStrings::Output::Backbuffer)
		|| IsEqual(InResourceName, UE::TextureShare::DisplayClusterStrings::Output::BackbufferTemp))
		{
			return ETextureShareSyncStep::FrameProxyPostRenderEnd;
		}
		else if (IsEqual(InResourceName, UE::TextureShare::DisplayClusterStrings::Viewport::Input) || IsEqual(InResourceName, UE::TextureShare::DisplayClusterStrings::Viewport::Mips))
		{
			return ETextureShareSyncStep::FrameProxyRenderEnd;
		}
		else if (IsEqual(InResourceName, UE::TextureShare::DisplayClusterStrings::Viewport::Warped))
		{
			return ETextureShareSyncStep::FrameProxyPostWarpEnd;
		}
		else if (IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::Backbuffer))
		{
			return ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentEnd;
		}

		// Default sync steps
		return ETextureShareSyncStep::FrameProxyPreRenderEnd;
	}

	static ETextureShareSyncStep GeSyncStep(const wchar_t* InResourceName, const ETextureShareTextureOp InOperationType)
	{
		// Display cluster sync steps for resources:
		if(IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::SceneColor)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::SceneDepth)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::SmallDepthZ)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::GBufferA)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::GBufferB)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::GBufferC)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::GBufferD)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::GBufferE)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::GBufferF)
		|| IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::FinalColor))
		{
			return ETextureShareSyncStep::FrameSceneFinalColorEnd;
		}
		else if (IsEqual(InResourceName, UE::TextureShareStrings::SceneTextures::Backbuffer))
		{
			return ETextureShareSyncStep::FrameProxyBackBufferReadyToPresentEnd;
		}

		// Default sync steps
		return ETextureShareSyncStep::FrameProxyPreRenderEnd;
	}
}
