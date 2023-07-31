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
		static constexpr auto Viewport3 = TEXT("VP_3");
		static constexpr auto Viewport4 = TEXT("VP_4");
	}

	namespace Receive
	{
		namespace Texture1
		{
			// Request to read a resource #1 to a remote process
			static FTextureShareViewportResourceDesc Desc(DisplayCluster::Viewport3, TextureShareDisplayClusterStrings::Viewport::FinalColor, ETextureShareTextureOp::Read);

			// Container for receive: Texture size are not defined on the user side (values on the UE side are used)
			static FTextureShareResourceD3D12 Resource(EResourceSRV::texture1);
		}

		namespace Texture2
		{
			// Request to read a resource #2 to a remote process
			static FTextureShareViewportResourceDesc Desc(DisplayCluster::Viewport3, TextureShareDisplayClusterStrings::Viewport::Warped, ETextureShareTextureOp::Read);

			// Container for receive: Texture size are not defined on the user side (values on the UE side are used)
			static FTextureShareResourceD3D12 Resource(EResourceSRV::texture2);
		}
	}

	namespace Send
	{
		namespace Backbuffer
		{
			// Request for write to vp_3
			static FTextureShareViewportResourceDesc Desc(DisplayCluster::Viewport4, TextureShareDisplayClusterStrings::Viewport::Warped, ETextureShareTextureOp::Write);
		}
	}
};
