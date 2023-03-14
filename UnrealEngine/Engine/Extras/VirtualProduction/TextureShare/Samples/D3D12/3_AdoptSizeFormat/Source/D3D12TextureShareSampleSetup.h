// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"
#include "D3D12AppSetup.h"

namespace TextureShareSample
{
	static FTextureShareObjectDesc ObjectDesc(ETextureShareDeviceType::D3D12);

	namespace Receive
	{
		namespace Texture1
		{
			// Request to read a resource #1 to a remote process
			static FTextureShareResourceDesc Desc(TextureShareStrings::SceneTextures::SceneColor, ETextureShareTextureOp::Read);

			// Container for receive: Custom size are defined on the user's side
			static FTextureShareResourceD3D12 Resource(EResourceSRV::texture1, FIntPoint(640, 420));
		}

		namespace Texture2
		{
			// Request to read a resource #2 to a remote process
			static FTextureShareResourceDesc Desc(TextureShareStrings::SceneTextures::FinalColor, ETextureShareTextureOp::Read);

			// Container for receive: Custom size&format
			static FTextureShareResourceD3D12 Resource(EResourceSRV::texture2, FIntPoint(64, 64), DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UINT);
		}
	}
};
