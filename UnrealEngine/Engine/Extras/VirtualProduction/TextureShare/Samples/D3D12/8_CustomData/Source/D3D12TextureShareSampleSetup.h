// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"
#include "D3D12AppSetup.h"

namespace TextureShareSample
{
	static FTextureShareObjectDesc ObjectDesc(ETextureShareDeviceType::D3D12);

	namespace CustomData
	{
		static constexpr auto WriteParam1Key = TEXT("MySDKParam1");
		static constexpr auto WriteParam1Value = TEXT("Custom value for MyParam1");

		static constexpr auto WriteParam2Key = TEXT("MySDKParam2");
		static constexpr auto WriteParam2Value = TEXT("Custom value for MyParam2");

		static constexpr auto ReadParam1Key = TEXT("UE_TextureShare_Param1");
		static constexpr auto ReadParam2Key = TEXT("UE_TextureShare_Param2");
	}

	namespace Receive
	{
		namespace Texture1
		{
			// Request to read a resource #1 to a remote process
			static FTextureShareResourceDesc Desc(TextureShareStrings::SceneTextures::SceneColor, ETextureShareTextureOp::Read);

			// Container for receive: Texture size are not defined on the user side (values on the UE side are used)
			static FTextureShareResourceD3D12 Resource(EResourceSRV::texture1);
		}

		namespace Texture2
		{
			// Request to read a resource #2 to a remote process
			static FTextureShareResourceDesc Desc(TextureShareStrings::SceneTextures::FinalColor, ETextureShareTextureOp::Read);

			// Container for receive: Texture size are not defined on the user side (values on the UE side are used)
			static FTextureShareResourceD3D12 Resource(EResourceSRV::texture2);
		}
	}

	namespace Send
	{
		namespace Backbuffer
		{
#if 0
			// Request for write to FinalColor resource
			static FTextureShareResourceDesc Desc(TextureShareStrings::SceneTextures::FinalColor, ETextureShareTextureOp::Write);
#else
			// or to RTT texture
			static constexpr auto Name = TEXT("RTT_TextureShare");
			static FTextureShareResourceDesc Desc(Name, ETextureShareTextureOp::Write);
#endif
		}
	}
};
