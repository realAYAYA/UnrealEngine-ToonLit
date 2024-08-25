// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoDecoder.h"
#include "Video/CodecUtils/CodecUtilsH265.h"

/*
 * Configuration settings for H265 decoders.
 */
struct AVCODECSCORE_API FVideoDecoderConfigH265 : public FVideoDecoderConfig
{
    TMap<uint32, UE::AVCodecCore::H265::VPS_t> VPS;
	TMap<uint32, UE::AVCodecCore::H265::SPS_t> SPS;
	TMap<uint32, UE::AVCodecCore::H265::PPS_t> PPS;
	TArray<UE::AVCodecCore::H265::SEI_t> SEI;

	FVideoDecoderConfigH265(EAVPreset Preset = EAVPreset::Default)
		: FVideoDecoderConfig(Preset)
	{
	}

	FAVResult Parse(FVideoPacket const& Packet, TArray<UE::AVCodecCore::H265::Slice_t>& OutSlices);

    TOptional<int> GetLastSliceQP(TArray<UE::AVCodecCore::H265::Slice_t>& Slices);
};

DECLARE_TYPEID(FVideoDecoderConfigH265, AVCODECSCORE_API);
