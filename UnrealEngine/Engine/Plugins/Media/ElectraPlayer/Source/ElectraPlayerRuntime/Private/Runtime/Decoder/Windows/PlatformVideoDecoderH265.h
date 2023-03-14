// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Decoder/VideoDecoderH265.h"

namespace Electra
{
	/**
	 * H265 video decoder.
	**/
	class FPlatformVideoDecoderH265
	{
	public:
		static bool GetPlatformStreamDecodeCapability(IVideoDecoderH265::FStreamDecodeCapability& OutResult, const IVideoDecoderH265::FStreamDecodeCapability& InStreamParameter);
	};


} // namespace Electra
