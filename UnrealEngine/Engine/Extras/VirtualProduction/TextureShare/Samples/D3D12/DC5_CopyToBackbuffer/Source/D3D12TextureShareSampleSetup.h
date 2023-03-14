// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"
#include "D3D12AppSetup.h"

namespace TextureShareSample
{
	static FTextureShareObjectDesc ObjectDesc(ETextureShareDeviceType::D3D12, TextureShareDisplayClusterStrings::Default::ShareName);

	namespace DisplayCluster
	{
		static constexpr auto Viewport1 = TEXT("VP_0");
		static constexpr auto Viewport2 = TEXT("VP_1");
	}

	namespace Receive
	{
		namespace Texture1
		{
			// Request to read a resource #1 to a remote process
			static FTextureShareViewportResourceDesc Desc(DisplayCluster::Viewport1, TextureShareStrings::SceneTextures::FinalColor, ETextureShareTextureOp::Read);
		}

		namespace Texture2
		{
			// Request to read a resource #2 to a remote process
			static FTextureShareViewportResourceDesc Desc(DisplayCluster::Viewport2, TextureShareDisplayClusterStrings::Output::Backbuffer, ETextureShareTextureOp::Read);
		}
	}
};
