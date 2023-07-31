// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"
#include "D3D11AppSetup.h"

namespace TextureShareSample
{
	static FTextureShareObjectDesc ObjectDesc(ETextureShareDeviceType::D3D11, TextureShareDisplayClusterStrings::Default::ShareName);

	namespace DisplayCluster
	{
		static constexpr auto Viewport1 = TEXT("VP_0");
		static constexpr auto Viewport2 = TEXT("VP_1");
		static constexpr auto Viewport3 = TEXT("VP_3");
	}

	namespace Receive
	{
		namespace Texture1
		{
			// Request to read a resource #1 to a remote process
			static FTextureShareViewportResourceDesc Desc(DisplayCluster::Viewport1, TextureShareDisplayClusterStrings::Viewport::Warped, ETextureShareTextureOp::Read);

			// Container for receive: Texture size are not defined on the user side (values on the UE side are used)
			static FTextureShareResourceD3D11 Resource;
		}

		namespace Texture2
		{
			// Request to read a resource #2 to a remote process
			static FTextureShareViewportResourceDesc Desc(DisplayCluster::Viewport2, TextureShareDisplayClusterStrings::Viewport::Warped, ETextureShareTextureOp::Read);

			// Container for receive: Texture size are not defined on the user side (values on the UE side are used)
			static FTextureShareResourceD3D11 Resource;
		}
	}

	namespace Send
	{
		namespace Backbuffer
		{
			// Request for write to vp_3
			static FTextureShareViewportResourceDesc Desc(DisplayCluster::Viewport3, TextureShareDisplayClusterStrings::Viewport::Warped, ETextureShareTextureOp::Write);
		}
	}
};
