// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"
#include "D3D11AppSetup.h"

namespace TextureShareSample
{
	static FTextureShareObjectDesc ObjectDesc(ETextureShareDeviceType::D3D11);

	namespace Receive
	{
		namespace Texture1
		{
			// Request to read a resource #1 to a remote process
			static FTextureShareResourceDesc Desc(TextureShareStrings::SceneTextures::FinalColor, ETextureShareTextureOp::Read);
		}

		namespace Texture2
		{
			// Request to read a resource #2 to a remote process
			static FTextureShareResourceDesc Desc(TextureShareStrings::SceneTextures::Backbuffer, ETextureShareTextureOp::Read);
		}
	}
};
