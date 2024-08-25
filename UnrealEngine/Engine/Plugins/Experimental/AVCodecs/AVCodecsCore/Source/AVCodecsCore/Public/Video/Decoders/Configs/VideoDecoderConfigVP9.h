// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoDecoder.h"
#include "Video/CodecUtils/CodecUtilsVP9.h"

/*
 * Configuration settings for VP9 decoders.
 */
struct AVCODECSCORE_API FVideoDecoderConfigVP9 : public FVideoDecoderConfig
{
    FVideoDecoderConfigVP9(EAVPreset Preset = EAVPreset::Default)
		: FVideoDecoderConfig(Preset)
	{
	}

	FAVResult Parse(FVideoPacket const& Packet, UE::AVCodecCore::VP9::Header_t& OutHeader);
};

DECLARE_TYPEID(FVideoDecoderConfigVP9, AVCODECSCORE_API);
