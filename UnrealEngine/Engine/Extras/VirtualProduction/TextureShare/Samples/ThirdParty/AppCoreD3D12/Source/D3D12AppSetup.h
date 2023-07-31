// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareObject.h"

#if !defined TEXTURESHARE_PROJECT_NAME
#define TEXTURESHARE_PROJECT_NAME "D3D12 TextureShare Sample"
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
namespace D3D12AppSetup
{
	static constexpr auto AppName = TEXTURESHARE_PROJECT_NAME;

	namespace Backbuffer
	{
#if 1
		static FIntPoint Size(1920, 1080);
#else
		//2K
		static FIntPoint Size(2560, 1440);
#endif

		static constexpr DXGI_FORMAT Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	}
};
