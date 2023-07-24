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
			static constexpr auto Name = TEXT("Texture1");

			// Request to read a resource #1 to a remote process
			static FTextureShareResourceDesc Desc(Name, ETextureShareTextureOp::Read);

			// Container for receive: Texture size are not defined on the user side (values on the UE side are used)
			static FTextureShareResourceD3D11 Resource;
		}

		namespace Texture2
		{
			static constexpr auto Name = TEXT("Texture2");

			// Request to read a resource #2 to a remote process
			static FTextureShareResourceDesc Desc(Name, ETextureShareTextureOp::Read);

			// Container for receive: Texture size are not defined on the user side (values on the UE side are used)
			static FTextureShareResourceD3D11 Resource;
		}
	}

	namespace Send
	{
		namespace Backbuffer
		{
			static constexpr auto Name = TEXT("RTT_TextureShare");

			// Request for write to FinalColor resource
			static FTextureShareResourceDesc Desc(Name, ETextureShareTextureOp::Write);
		}
	}
};
