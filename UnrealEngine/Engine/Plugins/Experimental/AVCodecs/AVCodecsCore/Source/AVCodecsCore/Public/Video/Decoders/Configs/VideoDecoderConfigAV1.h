// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoDecoder.h"
#include "Video/CodecUtils/CodecUtilsAV1.h"

/*
 * Configuration settings for AV1 decoders.
 */
struct AVCODECSCORE_API FVideoDecoderConfigAV1 : public FVideoDecoderConfig
{
    FVideoDecoderConfigAV1(EAVPreset Preset = EAVPreset::Default)
		: FVideoDecoderConfig(Preset)
	{
	}
};

DECLARE_TYPEID(FVideoDecoderConfigAV1, AVCODECSCORE_API);
